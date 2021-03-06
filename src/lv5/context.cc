#include <cmath>
#include "cstring"
#include "ustring.h"
#include "jsfunction.h"
#include "arguments.h"
#include "context.h"
#include "interpreter.h"
#include "property.h"
#include "class.h"
#include "runtime.h"
#include "jsast.h"
#include "jsscript.h"
#include "jserror.h"
namespace iv {
namespace lv5 {
namespace {

const std::string length_string("length");
const std::string eval_string("eval");
const std::string arguments_string("arguments");
const std::string caller_string("caller");
const std::string callee_string("callee");
const std::string toString_string("toString");
const std::string valueOf_string("valueOf");
const std::string prototype_string("prototype");
const std::string constructor_string("constructor");

class ScriptScope : private core::Noncopyable<ScriptScope>::type {
 public:
  ScriptScope(Context* ctx, JSScript* script)
    : ctx_(ctx),
      prev_(ctx->current_script()) {
    ctx_->set_current_script(script);
  }
  ~ScriptScope() {
    ctx_->set_current_script(prev_);
  }
 private:
  Context* ctx_;
  JSScript* prev_;
};

}  // namespace

Context::Context()
  : global_obj_(),
    throw_type_error_(),
    lexical_env_(NULL),
    variable_env_(NULL),
    global_env_(NULL),
    binding_(&global_obj_),
    table_(),
    interp_(),
    mode_(NORMAL),
    ret_(),
    target_(NULL),
    error_(),
    builtins_(),
    strict_(false),
    generate_script_counter_(0),
    random_engine_(random_engine_type(),
                   random_distribution_type(0, 1)),
    length_symbol_(Intern(length_string)),
    eval_symbol_(Intern(eval_string)),
    arguments_symbol_(Intern(arguments_string)),
    caller_symbol_(Intern(caller_string)),
    callee_symbol_(Intern(callee_string)),
    toString_symbol_(Intern(toString_string)),
    valueOf_symbol_(Intern(valueOf_string)),
    prototype_symbol_(Intern(prototype_string)),
    constructor_symbol_(Intern(constructor_string)),
    current_script_(NULL) {
  JSObjectEnv* const env = Interpreter::NewObjectEnvironment(this,
                                                             &global_obj_,
                                                             NULL);
  lexical_env_ = env;
  variable_env_ = env;
  global_env_ = env;
  env->set_provide_this(true);
  interp_.set_context(this);
  // discard random
  for (std::size_t i = 0; i < 20; ++i) {
    Random();
  }
  Initialize();
}

Symbol Context::Intern(const core::StringPiece& str) {
  return table_.Lookup(str);
}

Symbol Context::Intern(const core::UStringPiece& str) {
  return table_.Lookup(str);
}

Symbol Context::Intern(const Identifier& ident) {
  return ident.symbol();
}

double Context::Random() {
  return random_engine_();
}

JSString* Context::ToString(Symbol sym) {
  return table_.ToString(this, sym);
}

const core::UString& Context::GetContent(Symbol sym) const {
  return table_.GetContent(sym);
}

bool Context::InCurrentLabelSet(
    const AnonymousBreakableStatement* stmt) const {
  // AnonymousBreakableStatement has empty label at first
  return !target_ || stmt == target_;
}

bool Context::InCurrentLabelSet(
    const NamedOnlyBreakableStatement* stmt) const {
  return stmt == target_;
}

bool Context::Run(JSScript* script) {
  const ScriptScope scope(this, script);
  interp_.Run(script->function(), script->type() == JSScript::kEval);
  return error_;
}

JSVal Context::ErrorVal() {
  return JSError::Detail(this, &error_);
}

void Context::Initialize() {
  // Object and Function
  JSObject* const obj_proto = JSObject::NewPlain(this);

  // Function
  JSNativeFunction* const func_proto =
      JSNativeFunction::NewPlain(this, &runtime::FunctionPrototype, 0);
  func_proto->set_prototype(obj_proto);
  struct Class func_cls = {
    JSString::NewAsciiString(this, "Function"),
    NULL,
    func_proto
  };
  func_proto->set_cls(func_cls.name);
  const Symbol func_name = Intern("Function");
  builtins_[func_name] = func_cls;

  // section 15.2.2
  JSNativeFunction* const obj_constructor =
      JSNativeFunction::NewPlain(this, &runtime::ObjectConstructor, 1);

  struct Class obj_cls = {
    JSString::NewAsciiString(this, "Object"),
    obj_constructor,
    obj_proto
  };
  obj_proto->set_cls(obj_cls.name);
  const Symbol obj_name = Intern("Object");
  builtins_[obj_name] = obj_cls;

  // lazy initialization
  obj_constructor->set_cls(func_cls.name);
  obj_constructor->set_prototype(func_cls.prototype);
  obj_constructor->DefineOwnProperty(
      this, prototype_symbol_,
      DataDescriptor(obj_proto, PropertyDescriptor::NONE),
      false, NULL);
  obj_proto->DefineOwnProperty(
      this, constructor_symbol_,
      DataDescriptor(obj_constructor,
                     PropertyDescriptor::WRITABLE |
                     PropertyDescriptor::CONFIGURABLE),
      false, NULL);
  {
    JSNativeFunction* const func =
        JSNativeFunction::New(this, &runtime::FunctionToString, 0);
    func_proto->DefineOwnProperty(
        this, toString_symbol_,
        DataDescriptor(func,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
  }

  {
    // Object Define

    // section 15.2.3.2 Object.getPrototypeOf(O)
    obj_constructor->DefineOwnProperty(
        this, Intern("getPrototypeOf"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectGetPrototypeOf, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.3 Object.getOwnPropertyDescriptor(O, P)
    obj_constructor->DefineOwnProperty(
        this, Intern("getOwnPropertyDescriptor"),
        DataDescriptor(
            JSNativeFunction::New(this,
                                  &runtime::ObjectGetOwnPropertyDescriptor, 2),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.4 Object.getOwnPropertyNames(O)
    obj_constructor->DefineOwnProperty(
        this, Intern("getOwnPropertyNames"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectGetOwnPropertyNames, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.5 Object.create(O[, Properties])
    obj_constructor->DefineOwnProperty(
        this, Intern("create"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectCreate, 2),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.6 Object.defineProperty(O, P, Attributes)
    obj_constructor->DefineOwnProperty(
        this, Intern("defineProperty"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectDefineProperty, 3),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.7 Object.defineProperties(O, Properties)
    obj_constructor->DefineOwnProperty(
        this, Intern("defineProperties"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectDefineProperties, 2),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.8 Object.seal(O)
    obj_constructor->DefineOwnProperty(
        this, Intern("seal"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectSeal, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.9 Object.freeze(O)
    obj_constructor->DefineOwnProperty(
        this, Intern("freeze"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectFreeze, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.10 Object.preventExtensions(O)
    obj_constructor->DefineOwnProperty(
        this, Intern("preventExtensions"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectPreventExtensions, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.11 Object.isSealed(O)
    obj_constructor->DefineOwnProperty(
        this, Intern("isSealed"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectIsSealed, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.12 Object.isFrozen(O)
    obj_constructor->DefineOwnProperty(
        this, Intern("isFrozen"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectIsFrozen, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.13 Object.isExtensible(O)
    obj_constructor->DefineOwnProperty(
        this, Intern("isExtensible"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectIsExtensible, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.3.14 Object.keys(O)
    obj_constructor->DefineOwnProperty(
        this, Intern("keys"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectKeys, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.4.2 Object.prototype.toString()
    obj_proto->DefineOwnProperty(
        this, toString_symbol_,
        DataDescriptor(JSNativeFunction::New(this, &runtime::ObjectToString, 0),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.4.3 Object.prototype.toLocaleString()
    obj_proto->DefineOwnProperty(
        this, Intern("toLocaleString"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectToLocaleString, 0),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.4.4 Object.prototype.valueOf()
    obj_proto->DefineOwnProperty(
        this, Intern("valueOf"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectValueOf, 0),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.4.5 Object.prototype.hasOwnProperty(V)
    obj_proto->DefineOwnProperty(
        this, Intern("hasOwnProperty"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectHasOwnProperty, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.4.6 Object.prototype.isPrototypeOf(V)
    obj_proto->DefineOwnProperty(
        this, Intern("isPrototypeOf"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::ObjectIsPrototypeOf, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.2.4.7 Object.prototype.propertyIsEnumerable(V)
    obj_proto->DefineOwnProperty(
        this, Intern("propertyIsEnumerable"),
        DataDescriptor(
            JSNativeFunction::New(
                this, &runtime::ObjectPropertyIsEnumerable, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    global_obj_.DefineOwnProperty(
        this, obj_name,
        DataDescriptor(obj_constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
  }

  {
    // Array
    // TODO(Constellation) more
    JSObject* const proto = JSObject::NewPlain(this);
    // section 15.4.2 The Array Constructor
    JSNativeFunction* const constructor =
        JSNativeFunction::NewPlain(this, &runtime::ArrayConstructor, 1);
    constructor->set_cls(func_cls.name);
    constructor->set_prototype(func_cls.prototype);

    // set prototype
    constructor->DefineOwnProperty(
        this, prototype_symbol_,
        DataDescriptor(proto, PropertyDescriptor::NONE),
        false, NULL);
    proto->set_prototype(obj_proto);
    struct Class cls = {
      JSString::NewAsciiString(this, "Array"),
      constructor,
      proto
    };
    proto->set_cls(cls.name);

    const Symbol name = Intern("Array");
    builtins_[name] = cls;
    global_obj_.DefineOwnProperty(
        this, name,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    proto->DefineOwnProperty(
        this, constructor_symbol_,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
  }

  {
    // String
    // TODO(Constellation) more
    JSStringObject* const proto = JSStringObject::NewPlain(this);

    // section 15.5.2 The String Constructor
    JSNativeFunction* const constructor =
        JSNativeFunction::NewPlain(this, &runtime::StringConstructor, 1);
    constructor->set_cls(func_cls.name);
    constructor->set_prototype(func_cls.prototype);

    // set prototype
    constructor->DefineOwnProperty(
        this, prototype_symbol_,
        DataDescriptor(proto, PropertyDescriptor::NONE),
        false, NULL);
    proto->set_prototype(obj_proto);
    struct Class cls = {
      JSString::NewAsciiString(this, "String"),
      constructor,
      proto
    };
    proto->set_cls(cls.name);

    const Symbol name = Intern("String");
    builtins_[name] = cls;
    global_obj_.DefineOwnProperty(
        this, name,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    proto->DefineOwnProperty(
        this, constructor_symbol_,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.5.3.2 String.fromCharCode([char0 [, char1[, ...]]])
    constructor->DefineOwnProperty(
        this, Intern("fromCharCode"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::StringFromCharCode, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    {
      // section 15.5.4.2 String.prototype.toString()
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::StringToString, 0);
      proto->DefineOwnProperty(
          this, toString_symbol_,
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.5.4.3 String.prototype.valueOf()
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::StringValueOf, 0);
      proto->DefineOwnProperty(
          this, valueOf_symbol_,
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.5.4.4 String.prototype.charAt(pos)
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::StringCharAt, 1);
      proto->DefineOwnProperty(
          this, Intern("charAt"),
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.5.4.5 String.prototype.charCodeAt(pos)
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::StringCharCodeAt, 1);
      proto->DefineOwnProperty(
          this, Intern("charCodeAt"),
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.5.4.6 String.prototype.concat([string1[, string2[, ...]]])
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::StringConcat, 1);
      proto->DefineOwnProperty(
          this, Intern("concat"),
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.5.4.7 String.prototype.indexOf(searchString, position)
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::StringIndexOf, 1);
      proto->DefineOwnProperty(
          this, Intern("indexOf"),
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.5.4.8 String.prototype.lastIndexOf(searchString, position)
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::StringLastIndexOf, 1);
      proto->DefineOwnProperty(
          this, Intern("lastIndexOf"),
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
  }

  {
    // Boolean
    JSBooleanObject* const proto = JSBooleanObject::NewPlain(this, false);

    // section 15.5.2 The Boolean Constructor
    JSNativeFunction* const constructor =
        JSNativeFunction::NewPlain(this, &runtime::BooleanConstructor, 1);
    constructor->set_cls(func_cls.name);
    constructor->set_prototype(func_cls.prototype);

    // set prototype
    constructor->DefineOwnProperty(
        this, prototype_symbol_,
        DataDescriptor(proto, PropertyDescriptor::NONE),
        false, NULL);
    proto->set_prototype(obj_proto);
    struct Class cls = {
      JSString::NewAsciiString(this, "Boolean"),
      constructor,
      proto
    };
    proto->set_cls(cls.name);

    const Symbol name = Intern("Boolean");
    builtins_[name] = cls;
    global_obj_.DefineOwnProperty(
        this, name,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    proto->DefineOwnProperty(
        this, constructor_symbol_,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    {
      // section 15.6.4.2 Boolean.prototype.toString()
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::BooleanToString, 0);
      proto->DefineOwnProperty(
          this, toString_symbol_,
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.6.4.3 Boolean.prototype.valueOf()
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::BooleanValueOf, 0);
      proto->DefineOwnProperty(
          this, valueOf_symbol_,
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
  }

  {
    // Number
    JSNumberObject* const proto = JSNumberObject::NewPlain(this, 0);

    // section 15.7.3 The Number Constructor
    JSNativeFunction* const constructor =
        JSNativeFunction::NewPlain(this, &runtime::NumberConstructor, 1);
    constructor->set_cls(func_cls.name);
    constructor->set_prototype(func_cls.prototype);

    // set prototype
    constructor->DefineOwnProperty(
        this, prototype_symbol_,
        DataDescriptor(proto, PropertyDescriptor::NONE),
        false, NULL);
    proto->set_prototype(obj_proto);
    struct Class cls = {
      JSString::NewAsciiString(this, "Number"),
      constructor,
      proto
    };
    proto->set_cls(cls.name);

    const Symbol name = Intern("Number");
    builtins_[name] = cls;
    global_obj_.DefineOwnProperty(
        this, name,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.7.3.2 Number.MAX_VALUE
    constructor->DefineOwnProperty(
        this, Intern("MAX_VALUE"),
        DataDescriptor(1.7976931348623157e+308,
                       PropertyDescriptor::NONE),
        false, NULL);

    // section 15.7.3.3 Number.MIN_VALUE
    constructor->DefineOwnProperty(
        this, Intern("MIN_VALUE"),
        DataDescriptor(5e-324,
                       PropertyDescriptor::NONE),
        false, NULL);

    // section 15.7.3.4 Number.NaN
    constructor->DefineOwnProperty(
        this, Intern("NaN"),
        DataDescriptor(JSValData::kNaN,
                       PropertyDescriptor::NONE),
        false, NULL);

    // section 15.7.3.5 Number.NEGATIVE_INFINITY
    constructor->DefineOwnProperty(
        this, Intern("NEGATIVE_INFINITY"),
        DataDescriptor(-std::numeric_limits<double>::infinity(),
                       PropertyDescriptor::NONE),
        false, NULL);

    // section 15.7.3.6 Number.POSITIVE_INFINITY
    constructor->DefineOwnProperty(
        this, Intern("POSITIVE_INFINITY"),
        DataDescriptor(std::numeric_limits<double>::infinity(),
                       PropertyDescriptor::NONE),
        false, NULL);

    // section 15.7.4.1 Number.prototype.constructor
    proto->DefineOwnProperty(
        this, constructor_symbol_,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    {
      // section 15.7.4.2 Number.prototype.toString([radix])
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::NumberToString, 1);
      proto->DefineOwnProperty(
          this, toString_symbol_,
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.7.4.4 Number.prototype.toString([radix])
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::NumberValueOf, 0);
      proto->DefineOwnProperty(
          this, valueOf_symbol_,
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
  }

  {
    // Error
    JSObject* const proto = JSObject::NewPlain(this);
    // section 15.11.2 The Error Constructor
    JSNativeFunction* const constructor =
        JSNativeFunction::NewPlain(this, &runtime::ErrorConstructor, 1);
    constructor->set_cls(func_cls.name);
    constructor->set_prototype(func_cls.prototype);
    // set prototype
    constructor->DefineOwnProperty(
        this, prototype_symbol_,
        DataDescriptor(proto, PropertyDescriptor::NONE),
        false, NULL);
    proto->set_prototype(obj_proto);
    struct Class cls = {
      JSString::NewAsciiString(this, "Error"),
      constructor,
      proto
    };
    proto->set_cls(cls.name);
    const Symbol name = Intern("Error");
    builtins_[name] = cls;
    proto->DefineOwnProperty(
        this, constructor_symbol_,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    {
      // section 15.11.4.4 Error.prototype.toString()
      JSNativeFunction* const func =
          JSNativeFunction::New(this, &runtime::ErrorToString, 0);
      proto->DefineOwnProperty(
          this, toString_symbol_,
          DataDescriptor(func,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }

    // section 15.11.4.2 Error.prototype.name
    proto->DefineOwnProperty(
        this, Intern("name"),
        DataDescriptor(
            JSString::NewAsciiString(this, "Error"), PropertyDescriptor::NONE),
        false, NULL);

    // section 15.11.4.3 Error.prototype.message
    proto->DefineOwnProperty(
        this, Intern("message"),
        DataDescriptor(
            JSString::NewAsciiString(this, ""), PropertyDescriptor::NONE),
        false, NULL);
    global_obj_.DefineOwnProperty(
        this, name,
        DataDescriptor(constructor,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    {
      // section 15.11.6.1 EvalError
      JSObject* const sub_proto = JSObject::NewPlain(this);
      JSNativeFunction* const sub_constructor =
          JSNativeFunction::NewPlain(this, &runtime::EvalErrorConstructor, 1);
      sub_constructor->set_cls(func_cls.name);
      sub_constructor->set_prototype(func_cls.prototype);
      // set prototype
      sub_constructor->DefineOwnProperty(
          this, prototype_symbol_,
          DataDescriptor(proto, PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->set_prototype(proto);
      struct Class sub_cls = {
        JSString::NewAsciiString(this, "EvalError"),
        sub_constructor,
        sub_proto
      };
      sub_proto->set_cls(sub_cls.name);
      const Symbol sub_name = Intern("EvalError");
      builtins_[sub_name] = sub_cls;
      global_obj_.DefineOwnProperty(
          this, sub_name,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, Intern("name"),
          DataDescriptor(
              JSString::NewAsciiString(this, "EvalError"),
              PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, constructor_symbol_,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.11.6.2 RangeError
      JSObject* const sub_proto = JSObject::NewPlain(this);
      JSNativeFunction* const sub_constructor =
          JSNativeFunction::NewPlain(this, &runtime::RangeErrorConstructor, 1);
      sub_constructor->set_cls(func_cls.name);
      sub_constructor->set_prototype(func_cls.prototype);
      // set prototype
      sub_constructor->DefineOwnProperty(
          this, prototype_symbol_,
          DataDescriptor(proto, PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->set_prototype(proto);
      struct Class sub_cls = {
        JSString::NewAsciiString(this, "RangeError"),
        sub_constructor,
        sub_proto
      };
      sub_proto->set_cls(sub_cls.name);
      const Symbol sub_name = Intern("RangeError");
      builtins_[sub_name] = sub_cls;
      global_obj_.DefineOwnProperty(
          this, sub_name,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, Intern("name"),
          DataDescriptor(
              JSString::NewAsciiString(this, "RangeError"),
              PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, constructor_symbol_,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.11.6.3 ReferenceError
      JSObject* const sub_proto = JSObject::NewPlain(this);
      JSNativeFunction* const sub_constructor =
          JSNativeFunction::NewPlain(this,
                                     &runtime::ReferenceErrorConstructor, 1);
      sub_constructor->set_cls(func_cls.name);
      sub_constructor->set_prototype(func_cls.prototype);
      // set prototype
      sub_constructor->DefineOwnProperty(
          this, prototype_symbol_,
          DataDescriptor(proto, PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->set_prototype(proto);
      struct Class sub_cls = {
        JSString::NewAsciiString(this, "ReferenceError"),
        sub_constructor,
        sub_proto
      };
      sub_proto->set_cls(sub_cls.name);
      const Symbol sub_name = Intern("ReferenceError");
      builtins_[sub_name] = sub_cls;
      global_obj_.DefineOwnProperty(
          this, sub_name,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, Intern("name"),
          DataDescriptor(
              JSString::NewAsciiString(this, "ReferenceError"),
              PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, constructor_symbol_,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.11.6.4 SyntaxError
      JSObject* const sub_proto = JSObject::NewPlain(this);
      JSNativeFunction* const sub_constructor =
          JSNativeFunction::NewPlain(this, &runtime::SyntaxErrorConstructor, 1);
      sub_constructor->set_cls(func_cls.name);
      sub_constructor->set_prototype(func_cls.prototype);
      // set prototype
      sub_constructor->DefineOwnProperty(
          this, prototype_symbol_,
          DataDescriptor(proto, PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->set_prototype(proto);
      struct Class sub_cls = {
        JSString::NewAsciiString(this, "SyntaxError"),
        sub_constructor,
        sub_proto
      };
      sub_proto->set_cls(sub_cls.name);
      const Symbol sub_name = Intern("SyntaxError");
      builtins_[sub_name] = sub_cls;
      global_obj_.DefineOwnProperty(
          this, sub_name,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, Intern("name"),
          DataDescriptor(
              JSString::NewAsciiString(this, "SyntaxError"),
              PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, constructor_symbol_,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.11.6.5 TypeError
      JSObject* const sub_proto = JSObject::NewPlain(this);
      JSNativeFunction* const sub_constructor =
          JSNativeFunction::NewPlain(this, &runtime::TypeErrorConstructor, 1);
      sub_constructor->set_cls(func_cls.name);
      sub_constructor->set_prototype(func_cls.prototype);
      // set prototype
      sub_constructor->DefineOwnProperty(
          this, prototype_symbol_,
          DataDescriptor(proto, PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->set_prototype(proto);
      struct Class sub_cls = {
        JSString::NewAsciiString(this, "TypeError"),
        sub_constructor,
        sub_proto
      };
      sub_proto->set_cls(sub_cls.name);
      const Symbol sub_name = Intern("TypeError");
      builtins_[sub_name] = sub_cls;
      global_obj_.DefineOwnProperty(
          this, sub_name,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, Intern("name"),
          DataDescriptor(
              JSString::NewAsciiString(this, "TypeError"),
              PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, constructor_symbol_,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
    {
      // section 15.11.6.6 URIError
      JSObject* const sub_proto = JSObject::NewPlain(this);
      JSNativeFunction* const sub_constructor =
          JSNativeFunction::NewPlain(this, &runtime::URIErrorConstructor, 1);
      sub_constructor->set_cls(func_cls.name);
      sub_constructor->set_prototype(func_cls.prototype);
      // set prototype
      sub_constructor->DefineOwnProperty(
          this, prototype_symbol_,
          DataDescriptor(proto, PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->set_prototype(proto);
      struct Class sub_cls = {
        JSString::NewAsciiString(this, "URIError"),
        sub_constructor,
        sub_proto
      };
      sub_proto->set_cls(sub_cls.name);
      const Symbol sub_name = Intern("URIError");
      builtins_[sub_name] = sub_cls;
      global_obj_.DefineOwnProperty(
          this, sub_name,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, Intern("name"),
          DataDescriptor(
              JSString::NewAsciiString(this, "URIError"),
              PropertyDescriptor::NONE),
          false, NULL);
      sub_proto->DefineOwnProperty(
          this, constructor_symbol_,
          DataDescriptor(sub_constructor,
                         PropertyDescriptor::WRITABLE |
                         PropertyDescriptor::CONFIGURABLE),
          false, NULL);
    }
  }

  {
    // section 15.8 Math
    JSObject* const math = JSObject::NewPlain(this);
    math->set_prototype(obj_proto);
    math->set_cls(JSString::NewAsciiString(this, "Math"));
    global_obj_.DefineOwnProperty(
        this, Intern("Math"),
        DataDescriptor(math,
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.1.1 E
    math->DefineOwnProperty(
        this, Intern("E"),
        DataDescriptor(std::exp(1.0), PropertyDescriptor::NONE),
        false, NULL);

    // section 15.8.1.2 LN10
    math->DefineOwnProperty(
        this, Intern("LN10"),
        DataDescriptor(std::log(10.0), PropertyDescriptor::NONE),
        false, NULL);

    // section 15.8.1.3 LN2
    math->DefineOwnProperty(
        this, Intern("LN2"),
        DataDescriptor(std::log(2.0), PropertyDescriptor::NONE),
        false, NULL);

    // section 15.8.1.4 LOG2E
    math->DefineOwnProperty(
        this, Intern("LOG2E"),
        DataDescriptor(1.0 / std::log(2.0), PropertyDescriptor::NONE),
        false, NULL);

    // section 15.8.1.5 LOG10E
    math->DefineOwnProperty(
        this, Intern("LOG10E"),
        DataDescriptor(1.0 / std::log(10.0), PropertyDescriptor::NONE),
        false, NULL);

    // section 15.8.1.6 PI
    math->DefineOwnProperty(
        this, Intern("PI"),
        DataDescriptor(std::acos(-1.0), PropertyDescriptor::NONE),
        false, NULL);

    // section 15.8.1.7 SQRT1_2
    math->DefineOwnProperty(
        this, Intern("SQRT1_2"),
        DataDescriptor(std::sqrt(0.5), PropertyDescriptor::NONE),
        false, NULL);

    // section 15.8.1.8 SQRT2
    math->DefineOwnProperty(
        this, Intern("SQRT2"),
        DataDescriptor(std::sqrt(2.0), PropertyDescriptor::NONE),
        false, NULL);

    // section 15.8.2.1 abs(x)
    math->DefineOwnProperty(
        this, Intern("abs"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathAbs, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.2 acos(x)
    math->DefineOwnProperty(
        this, Intern("acos"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathAcos, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.3 asin(x)
    math->DefineOwnProperty(
        this, Intern("asin"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathAsin, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.4 atan(x)
    math->DefineOwnProperty(
        this, Intern("atan"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathAtan, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.5 atan2(y, x)
    math->DefineOwnProperty(
        this, Intern("atan2"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathAtan2, 2),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.6 ceil(x)
    math->DefineOwnProperty(
        this, Intern("ceil"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathCeil, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.7 cos(x)
    math->DefineOwnProperty(
        this, Intern("cos"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathCos, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.8 exp(x)
    math->DefineOwnProperty(
        this, Intern("exp"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathExp, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.9 floor(x)
    math->DefineOwnProperty(
        this, Intern("floor"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathFloor, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.10 log(x)
    math->DefineOwnProperty(
        this, Intern("log"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathLog, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.11 max([value1[, value2[, ... ]]])
    math->DefineOwnProperty(
        this, Intern("max"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathMax, 2),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.12 min([value1[, value2[, ... ]]])
    math->DefineOwnProperty(
        this, Intern("min"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathMin, 2),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.13 pow(x, y)
    math->DefineOwnProperty(
        this, Intern("pow"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathPow, 2),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.14 random()
    math->DefineOwnProperty(
        this, Intern("random"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathRandom, 0),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.15 round(x)
    math->DefineOwnProperty(
        this, Intern("round"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathRound, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.16 sin(x)
    math->DefineOwnProperty(
        this, Intern("sin"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathSin, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.17 sqrt(x)
    math->DefineOwnProperty(
        this, Intern("sqrt"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathSqrt, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);

    // section 15.8.2.18 tan(x)
    math->DefineOwnProperty(
        this, Intern("tan"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::MathTan, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
  }

  {
    // Builtins
    // section 15.1.1.1 NaN
    global_obj_.DefineOwnProperty(
        this, Intern("NaN"),
        DataDescriptor(
            JSValData::kNaN, PropertyDescriptor::NONE),
        false, NULL);
    // section 15.1.1.2 Infinity
    global_obj_.DefineOwnProperty(
        this, Intern("Infinity"),
        DataDescriptor(
            std::numeric_limits<double>::infinity(), PropertyDescriptor::NONE),
        false, NULL);
    // section 15.1.1.3 undefined
    global_obj_.DefineOwnProperty(
        this, Intern("undefined"),
        DataDescriptor(JSUndefined, PropertyDescriptor::NONE),
        false, NULL);
    // section 15.1.2.1 eval(x)
    global_obj_.DefineOwnProperty(
        this, Intern("eval"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::GlobalEval, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    // section 15.1.2.3 parseIng(string, radix)
    global_obj_.DefineOwnProperty(
        this, Intern("parseInt"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::GlobalParseInt, 2),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    // section 15.1.2.3 parseFloat(string)
    global_obj_.DefineOwnProperty(
        this, Intern("parseFloat"),
        DataDescriptor(
            JSNativeFunction::New(this, &runtime::GlobalParseFloat, 1),
            PropertyDescriptor::WRITABLE |
            PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    // section 15.1.2.4 isNaN(number)
    global_obj_.DefineOwnProperty(
        this, Intern("isNaN"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::GlobalIsNaN, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    // section 15.1.2.5 isFinite(number)
    global_obj_.DefineOwnProperty(
        this, Intern("isFinite"),
        DataDescriptor(JSNativeFunction::New(this, &runtime::GlobalIsFinite, 1),
                       PropertyDescriptor::WRITABLE |
                       PropertyDescriptor::CONFIGURABLE),
        false, NULL);
    global_obj_.set_cls(JSString::NewAsciiString(this, "global"));
    global_obj_.set_prototype(obj_proto);
  }

  {
    // Arguments
    struct Class cls = {
      JSString::NewAsciiString(this, "Arguments"),
      NULL,
      obj_proto
    };
    const Symbol name = Intern("Arguments");
    builtins_[name] = cls;
  }
  throw_type_error_.Initialize(this, &runtime::ThrowTypeError, 0);
}

const Class& Context::Cls(Symbol name) {
  assert(builtins_.find(name) != builtins_.end());
  return builtins_[name];
}

const Class& Context::Cls(const core::StringPiece& str) {
  assert(builtins_.find(Intern(str)) != builtins_.end());
  return builtins_[Intern(str)];
}

} }  // namespace iv::lv5
