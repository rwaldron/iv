#ifndef _IV_LV5_JSREGEXP_H_
#define _IV_LV5_JSREGEXP_H_
#include <unicode/regex.h>
#include <gc/gc_cpp.h>
#include "jsobject.h"
#include "stringpiece.h"
#include "ustringpiece.h"
namespace iv {

namespace core {
class RegExpLiteral;
}  // namespace iv::core

namespace lv5 {

class JSRegExpImpl : public gc_cleanup {
 public:
  JSRegExpImpl(const core::UStringPiece& value,
               const core::UStringPiece& flags,
               UErrorCode* status);
  ~JSRegExpImpl();
 private:
  URegularExpression* regexp_;
  bool global_;
};

class JSRegExp : public JSObject {
 public:
  JSRegExp(const core::UStringPiece& value,
           const core::UStringPiece& flags);
  explicit JSRegExp(const JSRegExpImpl& reg);

  inline bool IsValid() const {
    return status_ == U_ZERO_ERROR;
  }

  bool IsCallable() const {
    return true;
  }

  static JSRegExp* New(const core::UStringPiece& value,
                       const core::UStringPiece& flags);
  static JSRegExp* New(const core::RegExpLiteral* reg);

 private:
  UErrorCode status_;
  const JSRegExpImpl* impl_;
};

} }  // namespace iv::lv5
#endif  // _IV_LV5_JSREGEXP_H_
