#ifndef _IV_LV5_RUNTIME_BOOLEAN_H_
#define _IV_LV5_RUNTIME_BOOLEAN_H_
#include "arguments.h"
#include "jsval.h"
#include "error.h"
#include "jsobject.h"
#include "jsstring.h"
namespace iv {
namespace lv5 {
namespace runtime {

// section 15.6.1.1 Boolean(value)
// section 15.6.2.1 new Boolean(value)
inline JSVal BooleanConstructor(const Arguments& args, Error* error) {
  if (args.IsConstructorCalled()) {
    bool res = false;
    if (args.size() > 0) {
      res = args[0].ToBoolean(ERROR(error));
    }
    return JSBooleanObject::New(args.ctx(), res);
  } else {
    if (args.size() > 0) {
      const bool res = args[0].ToBoolean(ERROR(error));
      if (res) {
        return JSTrue;
      } else {
        return JSFalse;
      }
    } else {
      return JSFalse;
    }
  }
}

// section 15.6.4.2 Boolean.prototype.toString()
inline JSVal BooleanToString(const Arguments& args, Error* error) {
  CONSTRUCTOR_CHECK("Boolean.prototype.toString", args, error);
  const JSVal& obj = args.this_binding();
  bool b;
  if (!obj.IsBoolean()) {
    if (obj.IsObject() && obj.object()->AsBooleanObject()) {
      b = obj.object()->AsBooleanObject()->value();
    } else {
      error->Report(Error::Type,
                    "Boolean.prototype.toString is not generic function");
      return JSUndefined;
    }
  } else {
    b = obj.boolean();
  }
  return JSString::NewAsciiString(args.ctx(), (b) ? "true" : "false");
}

// section 15.6.4.3 Boolean.prototype.valueOf()
inline JSVal BooleanValueOf(const Arguments& args, Error* error) {
  CONSTRUCTOR_CHECK("Boolean.prototype.valueOf", args, error);
  const JSVal& obj = args.this_binding();
  bool b;
  if (!obj.IsBoolean()) {
    if (obj.IsObject() && obj.object()->AsBooleanObject()) {
      b = obj.object()->AsBooleanObject()->value();
    } else {
      error->Report(Error::Type,
                    "Boolean.prototype.valueOf is not generic function");
      return JSUndefined;
    }
  } else {
    b = obj.boolean();
  }
  return JSVal::Bool(b);
}

} } }  // namespace iv::lv5::runtime
#endif  // _IV_LV5_RUNTIME_STRING_H_
