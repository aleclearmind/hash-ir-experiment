#include <cstdint>
#include <map>
#include <set>
#include <iostream>

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"

#include "hashir.h"

// #define DEBUG

using namespace llvm;

char HashIRPass::ID = 0;
using Register = RegisterPass<HashIRPass>;
static Register X("hash-ir", "Inline Helpers Pass", true, true);

template<typename T>
uint64_t doHash(T Value) {
  unsigned long hash = 5381;
  for (char C : ArrayRef<char>(reinterpret_cast<char *>(&Value), sizeof(T))) {
    hash = (hash << 5) + hash + C; /* hash * 33 + c */
  }
  return hash;
}

class Hash;

#ifdef DEBUG

static size_t Depth = 0;

static void indent(size_t Amount) {
  for (size_t I = 0; I < Amount; ++I) {
    llvm::errs() << "  ";
  }
}

struct IncreaseDepth {
  IncreaseDepth() { ++Depth; }
  ~IncreaseDepth() { --Depth; }
};

#endif

class ScopedUpdater {
private:
  uint64_t NewValue;
  Hash *TheHash = nullptr;
#ifdef DEBUG
  IncreaseDepth X;
#endif

public:
  ScopedUpdater(uint64_t NewValue, Hash *TheHash) : NewValue(NewValue), TheHash(TheHash) {}
  ScopedUpdater() {}
  ~ScopedUpdater();
};

class Hash {
private:
  uint64_t Value = 0;

public:
  void update(uint64_t NewValue) {
#ifdef DEBUG
    indent(Depth + 1);
    llvm::errs() << NewValue;
    llvm::errs() << "\n";
#endif
    Value = Value ^ doHash(NewValue);
  }

  void update(Hash NewValue) { update(NewValue.Value); }
  ScopedUpdater scopedUpdate(uint64_t NewValue) {
    update(NewValue);
    return ScopedUpdater(NewValue, this);
  }

  uint64_t get() const { return Value; }

  bool operator<(const Hash &Other) const { return Value < Other.Value; }
};

ScopedUpdater::~ScopedUpdater() {
  if (TheHash != nullptr)
    TheHash->update(NewValue);
}

class Hasher {
private:
  std::map<Value *, Hash> Hashmap;
  std::set<Value *> InProgress;

public:
  uint64_t hashType(Type *T) {
    if (T->isVoidTy())
      return 0;
    auto *IT = cast<IntegerType>(T);
    return IT->getBitWidth();
  }

  Hash hash(Value *V) {
    // Memoize
    auto It = Hashmap.find(V);
    if (It != Hashmap.end())
      return It->second;

    // Prevent infinite recursion
    Hash Result;

#ifdef DEBUG
    IncreaseDepth X;
    indent(Depth - 1);
    V->print(llvm::errs());
    llvm::errs() << "\n";
#endif

    if (InProgress.count(V) != 0) {
      Result.update(4);
      return Result;
    }
    
    InProgress.insert(V);

    // Value type
    Result.update(hashType(V->getType()));

    if (auto *U = dyn_cast<User>(V)) {
      ScopedUpdater X;

      if (auto *I = dyn_cast<Instruction>(U)) {
        X = Result.scopedUpdate(0);
        Result.update(I->getOpcode());
      } else if (auto *CE = dyn_cast<ConstantExpr>(U)) {
        X = Result.scopedUpdate(1);
        Result.update(CE->getOpcode());
      } else if (auto *CI = dyn_cast<ConstantInt>(U)) {
        X = Result.scopedUpdate(2);
        // WIP: hash
        Result.update(CI->getLimitedValue());
      }

      // Operands count
      Result.update(U->getNumOperands());
      for (Value *V : U->operands())
        Result.update(hash(V));

    } else if (auto *A = dyn_cast<Argument>(V)) {
      Result.update(3);
    }

    Hashmap[V] = Result;

    InProgress.erase(V);

    return Result;
  }

  void dumpDuplicates() const {
    std::map<Hash, std::vector<Value *>> ReverseMap;
    for (auto [V, Hash] : Hashmap)
      ReverseMap[Hash].push_back(V);

    for (auto &[Hash, Values] : ReverseMap) {
      if (Values.size() > 1) {
        llvm::errs() << "Hash: " << Hash.get() << "\n";
        for (Value *V : Values) {
          llvm::errs() << "  ";
          V->print(llvm::errs());
          llvm::errs() << "\n";
        }
      }
    }
    
  }
};

bool HashIRPass::runOnModule(llvm::Module &M) {
  Hasher H;

  for (Function &F : M)
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        H.hash(&I);

  H.dumpDuplicates();
  
  return true;
}
