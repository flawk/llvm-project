//===- LLVMIRConversionGen.cpp - MLIR LLVM IR builder generator -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file uses tablegen definitions of the LLVM IR Dialect operations to
// generate the code building the LLVM IR from it.
//
//===----------------------------------------------------------------------===//

#include "mlir/Support/LogicalResult.h"
#include "mlir/TableGen/Argument.h"
#include "mlir/TableGen/Attribute.h"
#include "mlir/TableGen/GenInfo.h"
#include "mlir/TableGen/Operator.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"

using namespace llvm;
using namespace mlir;

static LogicalResult emitError(const Twine &message) {
  llvm::errs() << message << "\n";
  return failure();
}

namespace {
// Helper structure to return a position of the substring in a string.
struct StringLoc {
  size_t pos;
  size_t length;

  // Take a substring identified by this location in the given string.
  StringRef in(StringRef str) const { return str.substr(pos, length); }

  // A location is invalid if its position is outside the string.
  explicit operator bool() { return pos != std::string::npos; }
};
} // namespace

// Find the next TableGen variable in the given pattern.  These variables start
// with a `$` character and can contain alphanumeric characters or underscores.
// Return the position of the variable in the pattern and its length, including
// the `$` character.  The escape syntax `$$` is also detected and returned.
static StringLoc findNextVariable(StringRef str) {
  size_t startPos = str.find('$');
  if (startPos == std::string::npos)
    return {startPos, 0};

  // If we see "$$", return immediately.
  if (startPos != str.size() - 1 && str[startPos + 1] == '$')
    return {startPos, 2};

  // Otherwise, the symbol spans until the first character that is not
  // alphanumeric or '_'.
  size_t endPos = str.find_if_not([](char c) { return isAlnum(c) || c == '_'; },
                                  startPos + 1);
  if (endPos == std::string::npos)
    endPos = str.size();

  return {startPos, endPos - startPos};
}

// Check if `name` is the name of the variadic operand of `op`.  The variadic
// operand can only appear at the last position in the list of operands.
static bool isVariadicOperandName(const tblgen::Operator &op, StringRef name) {
  unsigned numOperands = op.getNumOperands();
  if (numOperands == 0)
    return false;
  const auto &operand = op.getOperand(numOperands - 1);
  return operand.isVariableLength() && operand.name == name;
}

// Check if `result` is a known name of a result of `op`.
static bool isResultName(const tblgen::Operator &op, StringRef name) {
  for (int i = 0, e = op.getNumResults(); i < e; ++i)
    if (op.getResultName(i) == name)
      return true;
  return false;
}

// Check if `name` is a known name of an attribute of `op`.
static bool isAttributeName(const tblgen::Operator &op, StringRef name) {
  return llvm::any_of(
      op.getAttributes(),
      [name](const tblgen::NamedAttribute &attr) { return attr.name == name; });
}

// Check if `name` is a known name of an operand of `op`.
static bool isOperandName(const tblgen::Operator &op, StringRef name) {
  for (int i = 0, e = op.getNumOperands(); i < e; ++i)
    if (op.getOperand(i).name == name)
      return true;
  return false;
}

// Return the `op` argument index of the argument with the given `name`.
static FailureOr<int> getArgumentIndex(const tblgen::Operator &op,
                                       StringRef name) {
  for (int i = 0, e = op.getNumArgs(); i != e; ++i)
    if (op.getArgName(i) == name)
      return i;
  return failure();
}

// Emit to `os` the operator-name driven check and the call to LLVM IRBuilder
// for one definition of an LLVM IR Dialect operation.
static LogicalResult emitOneBuilder(const Record &record, raw_ostream &os) {
  auto op = tblgen::Operator(record);

  if (!record.getValue("llvmBuilder"))
    return emitError("no 'llvmBuilder' field for op " + op.getOperationName());

  // Return early if there is no builder specified.
  StringRef builderStrRef = record.getValueAsString("llvmBuilder");
  if (builderStrRef.empty())
    return success();

  // Progressively create the builder string by replacing $-variables with
  // value lookups.  Keep only the not-yet-traversed part of the builder pattern
  // to avoid re-traversing the string multiple times.
  std::string builder;
  llvm::raw_string_ostream bs(builder);
  while (StringLoc loc = findNextVariable(builderStrRef)) {
    auto name = loc.in(builderStrRef).drop_front();
    auto getterName = op.getGetterName(name);
    // First, insert the non-matched part as is.
    bs << builderStrRef.substr(0, loc.pos);
    // Then, rewrite the name based on its kind.
    bool isVariadicOperand = isVariadicOperandName(op, name);
    if (isOperandName(op, name)) {
      auto result =
          isVariadicOperand
              ? formatv("moduleTranslation.lookupValues(op.{0}())", getterName)
              : formatv("moduleTranslation.lookupValue(op.{0}())", getterName);
      bs << result;
    } else if (isAttributeName(op, name)) {
      bs << formatv("op.{0}()", getterName);
    } else if (isResultName(op, name)) {
      bs << formatv("moduleTranslation.mapValue(op.{0}())", getterName);
    } else if (name == "_resultType") {
      bs << "moduleTranslation.convertType(op.getResult().getType())";
    } else if (name == "_hasResult") {
      bs << "opInst.getNumResults() == 1";
    } else if (name == "_location") {
      bs << "opInst.getLoc()";
    } else if (name == "_numOperands") {
      bs << "opInst.getNumOperands()";
    } else if (name == "$") {
      bs << '$';
    } else {
      return emitError(name + " is neither an argument nor a result of " +
                       op.getOperationName());
    }
    // Finally, only keep the untraversed part of the string.
    builderStrRef = builderStrRef.substr(loc.pos + loc.length);
  }

  // Output the check and the rewritten builder string.
  os << "if (auto op = dyn_cast<" << op.getQualCppClassName()
     << ">(opInst)) {\n";
  os << bs.str() << builderStrRef << "\n";
  os << "  return success();\n";
  os << "}\n";

  return success();
}

// Emit all builders.  Returns false on success because of the generator
// registration requirements.
static bool emitBuilders(const RecordKeeper &recordKeeper, raw_ostream &os) {
  for (const Record *def :
       recordKeeper.getAllDerivedDefinitions("LLVM_OpBase")) {
    if (failed(emitOneBuilder(*def, os)))
      return true;
  }
  return false;
}

// Emit an intrinsic identifier driven check and a call to the builder of the
// MLIR LLVM dialect intrinsic operation to build for the given LLVM IR
// intrinsic identifier.
static LogicalResult emitOneIntrBuilder(const Record &record, raw_ostream &os) {
  auto op = tblgen::Operator(record);

  if (!record.getValue("mlirBuilder"))
    return emitError("no 'mlirBuilder' field for op " + op.getOperationName());

  // Return early if there is no builder specified.
  StringRef builderStrRef = record.getValueAsString("mlirBuilder");
  if (builderStrRef.empty())
    return success();

  // Progressively create the builder string by replacing $-variables. Keep only
  // the not-yet-traversed part of the builder pattern to avoid re-traversing
  // the string multiple times.
  std::string builder;
  llvm::raw_string_ostream bs(builder);
  while (StringLoc loc = findNextVariable(builderStrRef)) {
    auto name = loc.in(builderStrRef).drop_front();
    // First, insert the non-matched part as is.
    bs << builderStrRef.substr(0, loc.pos);
    // Then, rewrite the name based on its kind.
    FailureOr<int> argIndex = getArgumentIndex(op, name);
    if (succeeded(argIndex)) {
      // Process the argument value assuming the MLIR and LLVM operand orders
      // match and there are no optional or variadic arguments.
      bs << formatv("processValue(llvmOperands[{0}])", *argIndex);
    } else if (isResultName(op, name)) {
      assert(op.getNumResults() == 1 &&
             "expected operation to have one result");
      bs << formatv("mapValue(inst)");
    } else if (name == "_int_attr") {
      bs << "matchIntegerAttr";
    } else if (name == "_resultType") {
      bs << "convertType(inst->getType())";
    } else if (name == "_location") {
      bs << "translateLoc(inst->getDebugLoc())";
    } else if (name == "_builder") {
      bs << "odsBuilder";
    } else if (name == "_qualCppClassName") {
      bs << op.getQualCppClassName();
    } else if (name == "$") {
      bs << '$';
    } else {
      return emitError(name +
                       " is not a known keyword, argument, or result of " +
                       op.getOperationName());
    }
    // Finally, only keep the untraversed part of the string.
    builderStrRef = builderStrRef.substr(loc.pos + loc.length);
  }

  // Output the check and the builder string.
  os << "if (intrinsicID == llvm::Intrinsic::"
     << record.getValueAsString("llvmEnumName") << ") {\n";
  os << bs.str() << builderStrRef << "\n";
  os << "  return success();\n";
  os << "}\n";

  return success();
}

// Emit all intrinsic builders. Returns false on success because of the
// generator registration requirements.
static bool emitIntrBuilders(const RecordKeeper &recordKeeper,
                             raw_ostream &os) {
  for (const Record *def :
       recordKeeper.getAllDerivedDefinitions("LLVM_IntrOpBase")) {
    if (failed(emitOneIntrBuilder(*def, os)))
      return true;
  }
  return false;
}

namespace {
// Wrapper class around a Tablegen definition of an LLVM enum attribute case.
class LLVMEnumAttrCase : public tblgen::EnumAttrCase {
public:
  using tblgen::EnumAttrCase::EnumAttrCase;

  // Constructs a case from a non LLVM-specific enum attribute case.
  explicit LLVMEnumAttrCase(const tblgen::EnumAttrCase &other)
      : tblgen::EnumAttrCase(&other.getDef()) {}

  // Returns the C++ enumerant for the LLVM API.
  StringRef getLLVMEnumerant() const {
    return def->getValueAsString("llvmEnumerant");
  }
};

// Wraper class around a Tablegen definition of an LLVM enum attribute.
class LLVMEnumAttr : public tblgen::EnumAttr {
public:
  using tblgen::EnumAttr::EnumAttr;

  // Returns the C++ enum name for the LLVM API.
  StringRef getLLVMClassName() const {
    return def->getValueAsString("llvmClassName");
  }

  // Returns all associated cases viewed as LLVM-specific enum cases.
  std::vector<LLVMEnumAttrCase> getAllCases() const {
    std::vector<LLVMEnumAttrCase> cases;

    for (auto &c : tblgen::EnumAttr::getAllCases())
      cases.emplace_back(c);

    return cases;
  }
};

// Wraper class around a Tablegen definition of a C-style LLVM enum attribute.
class LLVMCEnumAttr : public tblgen::EnumAttr {
public:
  using tblgen::EnumAttr::EnumAttr;

  // Returns the C++ enum name for the LLVM API.
  StringRef getLLVMClassName() const {
    return def->getValueAsString("llvmClassName");
  }

  // Returns all associated cases viewed as LLVM-specific enum cases.
  std::vector<LLVMEnumAttrCase> getAllCases() const {
    std::vector<LLVMEnumAttrCase> cases;

    for (auto &c : tblgen::EnumAttr::getAllCases())
      cases.emplace_back(c);

    return cases;
  }
};
} // namespace

// Emits conversion function "LLVMClass convertEnumToLLVM(Enum)" and containing
// switch-based logic to convert from the MLIR LLVM dialect enum attribute case
// (Enum) to the corresponding LLVM API enumerant
static void emitOneEnumToConversion(const llvm::Record *record,
                                    raw_ostream &os) {
  LLVMEnumAttr enumAttr(record);
  StringRef llvmClass = enumAttr.getLLVMClassName();
  StringRef cppClassName = enumAttr.getEnumClassName();
  StringRef cppNamespace = enumAttr.getCppNamespace();

  // Emit the function converting the enum attribute to its LLVM counterpart.
  os << formatv(
      "static LLVM_ATTRIBUTE_UNUSED {0} convert{1}ToLLVM({2}::{1} value) {{\n",
      llvmClass, cppClassName, cppNamespace);
  os << "  switch (value) {\n";

  for (const auto &enumerant : enumAttr.getAllCases()) {
    StringRef llvmEnumerant = enumerant.getLLVMEnumerant();
    StringRef cppEnumerant = enumerant.getSymbol();
    os << formatv("  case {0}::{1}::{2}:\n", cppNamespace, cppClassName,
                  cppEnumerant);
    os << formatv("    return {0}::{1};\n", llvmClass, llvmEnumerant);
  }

  os << "  }\n";
  os << formatv("  llvm_unreachable(\"unknown {0} type\");\n",
                enumAttr.getEnumClassName());
  os << "}\n\n";
}

// Emits conversion function "LLVMClass convertEnumToLLVM(Enum)" and containing
// switch-based logic to convert from the MLIR LLVM dialect enum attribute case
// (Enum) to the corresponding LLVM API C-style enumerant
static void emitOneCEnumToConversion(const llvm::Record *record,
                                     raw_ostream &os) {
  LLVMCEnumAttr enumAttr(record);
  StringRef llvmClass = enumAttr.getLLVMClassName();
  StringRef cppClassName = enumAttr.getEnumClassName();
  StringRef cppNamespace = enumAttr.getCppNamespace();

  // Emit the function converting the enum attribute to its LLVM counterpart.
  os << formatv("static LLVM_ATTRIBUTE_UNUSED int64_t "
                "convert{0}ToLLVM({1}::{0} value) {{\n",
                cppClassName, cppNamespace);
  os << "  switch (value) {\n";

  for (const auto &enumerant : enumAttr.getAllCases()) {
    StringRef llvmEnumerant = enumerant.getLLVMEnumerant();
    StringRef cppEnumerant = enumerant.getSymbol();
    os << formatv("  case {0}::{1}::{2}:\n", cppNamespace, cppClassName,
                  cppEnumerant);
    os << formatv("    return static_cast<int64_t>({0}::{1});\n", llvmClass,
                  llvmEnumerant);
  }

  os << "  }\n";
  os << formatv("  llvm_unreachable(\"unknown {0} type\");\n",
                enumAttr.getEnumClassName());
  os << "}\n\n";
}

// Emits conversion function "Enum convertEnumFromLLVM(LLVMClass)" and
// containing switch-based logic to convert from the LLVM API enumerant to MLIR
// LLVM dialect enum attribute (Enum).
static void emitOneEnumFromConversion(const llvm::Record *record,
                                      raw_ostream &os) {
  LLVMEnumAttr enumAttr(record);
  StringRef llvmClass = enumAttr.getLLVMClassName();
  StringRef cppClassName = enumAttr.getEnumClassName();
  StringRef cppNamespace = enumAttr.getCppNamespace();

  // Emit the function converting the enum attribute from its LLVM counterpart.
  os << formatv("inline LLVM_ATTRIBUTE_UNUSED {0}::{1} convert{1}FromLLVM({2} "
                "value) {{\n",
                cppNamespace, cppClassName, llvmClass);
  os << "  switch (value) {\n";

  for (const auto &enumerant : enumAttr.getAllCases()) {
    StringRef llvmEnumerant = enumerant.getLLVMEnumerant();
    StringRef cppEnumerant = enumerant.getSymbol();
    os << formatv("  case {0}::{1}:\n", llvmClass, llvmEnumerant);
    os << formatv("    return {0}::{1}::{2};\n", cppNamespace, cppClassName,
                  cppEnumerant);
  }

  os << "  }\n";
  os << formatv("  llvm_unreachable(\"unknown {0} type\");",
                enumAttr.getLLVMClassName());
  os << "}\n\n";
}

// Emits conversion function "Enum convertEnumFromLLVM(LLVMEnum)" and
// containing switch-based logic to convert from the LLVM API C-style enumerant
// to MLIR LLVM dialect enum attribute (Enum).
static void emitOneCEnumFromConversion(const llvm::Record *record,
                                       raw_ostream &os) {
  LLVMCEnumAttr enumAttr(record);
  StringRef llvmClass = enumAttr.getLLVMClassName();
  StringRef cppClassName = enumAttr.getEnumClassName();
  StringRef cppNamespace = enumAttr.getCppNamespace();

  // Emit the function converting the enum attribute from its LLVM counterpart.
  os << formatv(
      "inline LLVM_ATTRIBUTE_UNUSED {0}::{1} convert{1}FromLLVM(int64_t "
      "value) {{\n",
      cppNamespace, cppClassName, llvmClass);
  os << "  switch (value) {\n";

  for (const auto &enumerant : enumAttr.getAllCases()) {
    StringRef llvmEnumerant = enumerant.getLLVMEnumerant();
    StringRef cppEnumerant = enumerant.getSymbol();
    os << formatv("  case static_cast<int64_t>({0}::{1}):\n", llvmClass,
                  llvmEnumerant);
    os << formatv("    return {0}::{1}::{2};\n", cppNamespace, cppClassName,
                  cppEnumerant);
  }

  os << "  }\n";
  os << formatv("  llvm_unreachable(\"unknown {0} type\");",
                enumAttr.getLLVMClassName());
  os << "}\n\n";
}

// Emits conversion functions between MLIR enum attribute case and corresponding
// LLVM API enumerants for all registered LLVM dialect enum attributes.
template <bool ConvertTo>
static bool emitEnumConversionDefs(const RecordKeeper &recordKeeper,
                                   raw_ostream &os) {
  for (const Record *def :
       recordKeeper.getAllDerivedDefinitions("LLVM_EnumAttr"))
    if (ConvertTo)
      emitOneEnumToConversion(def, os);
    else
      emitOneEnumFromConversion(def, os);

  for (const Record *def :
       recordKeeper.getAllDerivedDefinitions("LLVM_CEnumAttr"))
    if (ConvertTo)
      emitOneCEnumToConversion(def, os);
    else
      emitOneCEnumFromConversion(def, os);

  return false;
}

static void emitOneIntrinsic(const Record &record, raw_ostream &os) {
  auto op = tblgen::Operator(record);
  os << "llvm::Intrinsic::" << record.getValueAsString("llvmEnumName") << ",\n";
}

// Emit the list of LLVM IR intrinsics identifiers that are convertible to a
// matching MLIR LLVM dialect intrinsic operation.
static bool emitConvertibleIntrinsics(const RecordKeeper &recordKeeper,
                                      raw_ostream &os) {
  for (const Record *def :
       recordKeeper.getAllDerivedDefinitions("LLVM_IntrOpBase"))
    emitOneIntrinsic(*def, os);

  return false;
}

static mlir::GenRegistration
    genLLVMIRConversions("gen-llvmir-conversions",
                         "Generate LLVM IR conversions", emitBuilders);

static mlir::GenRegistration
    genIntrFromLLVMIRConversions("gen-intr-from-llvmir-conversions",
                                 "Generate intrinsic conversions from LLVM IR",
                                 emitIntrBuilders);

static mlir::GenRegistration
    genEnumToLLVMConversion("gen-enum-to-llvmir-conversions",
                            "Generate conversions of EnumAttrs to LLVM IR",
                            emitEnumConversionDefs</*ConvertTo=*/true>);

static mlir::GenRegistration
    genEnumFromLLVMConversion("gen-enum-from-llvmir-conversions",
                              "Generate conversions of EnumAttrs from LLVM IR",
                              emitEnumConversionDefs</*ConvertTo=*/false>);

static mlir::GenRegistration genConvertibleLLVMIRIntrinsics(
    "gen-convertible-llvmir-intrinsics",
    "Generate list of convertible LLVM IR intrinsics",
    emitConvertibleIntrinsics);
