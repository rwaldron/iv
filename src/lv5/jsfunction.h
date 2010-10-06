#ifndef _IV_LV5_JSFUNCTION_H_
#define _IV_LV5_JSFUNCTION_H_
#include <tr1/functional>
#include "jserrorcode.h"
#include "jsenv.h"
#include "arguments.h"
#include "ustringpiece.h"
namespace iv {
namespace lv5 {

class Context;
class JSCodeFunction;
class JSNativeFunction;
class JSFunction : public JSObject {
 public:
  bool IsCallable() const {
    return true;
  }
  JSFunction* AsCallable() {
    return this;
  }
  virtual JSVal Call(const Arguments& args,
                     JSErrorCode::Type* error) = 0;
  virtual bool HasInstance(Context* context,
                           const JSVal& val, JSErrorCode::Type* error);
  virtual JSCodeFunction* AsCodeFunction() = 0;
  virtual JSNativeFunction* AsNativeFunction() = 0;
 protected:
  static void SetClass(Context* ctx, JSObject* obj);
};

class JSCodeFunction : public JSFunction {
 public:
  JSCodeFunction(core::FunctionLiteral* func, JSEnv* env);
  JSVal Call(const Arguments& args,
             JSErrorCode::Type* error);
  JSEnv* scope() const {
    return env_;
  }
  core::FunctionLiteral* code() const {
    return function_;
  }
  static JSCodeFunction* New(Context* ctx,
                             core::FunctionLiteral* func, JSEnv* env);
  JSCodeFunction* AsCodeFunction() {
    return this;
  }
  JSNativeFunction* AsNativeFunction() {
    return NULL;
  }
  core::UStringPiece GetSource() const {
    return function_->GetSource();
  }
  core::Identifier* name() const {
    return function_->name();
  }
 private:
  core::FunctionLiteral* function_;
  JSEnv* env_;
};

class JSNativeFunction : public JSFunction {
 public:
  template<typename Func>
  explicit JSNativeFunction(const Func& func)
    : func_(func) {
  }
  JSVal Call(const Arguments& args,
             JSErrorCode::Type* error);
  JSCodeFunction* AsCodeFunction() {
    return NULL;
  }
  JSNativeFunction* AsNativeFunction() {
    return this;
  }

  template<typename Func>
  static JSNativeFunction* New(Context* ctx, const Func& func) {
    JSNativeFunction* const obj = new JSNativeFunction(func);
    SetClass(ctx, obj);
    return obj;
  }

 private:
  std::tr1::function<JSVal(const Arguments&, JSErrorCode::Type*)> func_;
};
} }  // namespace iv::lv5
#endif  // _IV_LV5_JSFUNCTION_H_