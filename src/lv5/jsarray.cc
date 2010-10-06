#include <cstdlib>
#include <tr1/array>
#include <unicode/ustdio.h>
#include "conversions-inl.h"
#include "jsarray.h"
#include "jsobject.h"
#include "jsproperty.h"
#include "jsstring.h"
#include "context.h"
#include "class.h"

namespace iv {
namespace lv5 {

JSArray::JSArray(Context* ctx, std::size_t len)
  : JSObject(),
    length_(len) {
  JSObject::DefineOwnProperty(ctx, ctx->Intern("length"),
                              new DataDescriptor(JSVal(static_cast<double>(0)),
                                                 PropertyDescriptor::WRITABLE),
                                                 false, NULL);
}

#define REJECT()\
  do {\
    if (th) {\
      *res = JSErrorCode::TypeError;\
    }\
    return false;\
  } while (0)

bool JSArray::DefineOwnProperty(Context* ctx,
                                Symbol name,
                                const PropertyDescriptor* desc,
                                bool th,
                                JSErrorCode::Type* res) {
  const Symbol length_symbol = ctx->Intern("length");
  PropertyDescriptor* const old_len_desc_upper = GetOwnProperty(length_symbol);
  assert(old_len_desc_upper && old_len_desc_upper->AsDataDescriptor());
  DataDescriptor* const old_len_desc = old_len_desc_upper->AsDataDescriptor();

  const JSVal& len_value = old_len_desc->value();
  JSString* const name_string = ctx->ToString(name);

  if (name == length_symbol) {
    if (desc->AsDataDescriptor()) {
      const JSVal& new_len_value = desc->AsDataDescriptor()->value();
      const double new_len_double = new_len_value.ToNumber(ctx, res);
      if (*res) {
        return false;
      }
      uint32_t new_len = core::Conv::DoubleToUInt32(new_len_double);
      if (new_len != new_len_double) {
        *res = JSErrorCode::RangeError;
        return false;
      }
      double old_len = len_value.ToNumber(ctx, res);
      if (*res) {
        return false;
      }
      DataDescriptor* const new_len_desc = new DataDescriptor(
          JSVal(static_cast<double>(new_len)), desc->attrs());
      if (new_len >= old_len) {
        return JSObject::DefineOwnProperty(ctx, length_symbol,
                                           new_len_desc, th, res);
      }
      if (!old_len_desc->IsWritable()) {
        REJECT();
      }
      const bool new_writable =
          new_len_desc->IsWritableAbsent() || new_len_desc->IsWritable();
      if (!new_writable) {
        new_len_desc->SetWritable(true);
      }
      const bool succeeded = JSObject::DefineOwnProperty(ctx, length_symbol,
                                                         new_len_desc, th, res);
      if (!succeeded) {
        return false;
      }
      std::tr1::array<char, 80> buffer;
      while (new_len < old_len) {
        old_len -= 1;
        const char* const str = core::DoubleToCString(old_len,
                                                      buffer.data(),
                                                      buffer.size());
        const Symbol now_index = ctx->Intern(str);
        // see Eratta
        const bool delete_succeeded = Delete(now_index, false, res);
        if (*res) {
          return false;
        }
        if (!delete_succeeded) {
          new_len_desc->set_value(static_cast<double>(old_len + 1));
          if (!new_writable) {
            new_len_desc->SetWritable(false);
          }
          JSObject::DefineOwnProperty(ctx, length_symbol,
                                      new_len_desc, false, res);
          REJECT();
        }
      }
      if (!new_writable) {
        new_len_desc->SetWritable(false);
        JSObject::DefineOwnProperty(ctx, length_symbol,
                                    new_len_desc, false, res);
      }
      return true;
    } else {
      UNREACHABLE();
      return true;
    }
  } else {
    uint32_t index;
    if (u_sscanf(name_string->data(), "%u", &index) == 1) {
      std::tr1::array<char, 80> buffer;
      const char* const str = core::DoubleToCString(static_cast<double>(index),
                                                    buffer.data(),
                                                    buffer.size());
      const JSString* const converted = JSString::NewAsciiString(ctx, str);
      if (*converted == *name_string) {
        // array index
        const double old_len = len_value.ToNumber(ctx, res);
        if (*res) {
          return false;
        }
        if (index >= old_len && !old_len_desc->IsWritable()) {
          return false;
        }
        const bool succeeded = JSObject::DefineOwnProperty(ctx, name,
                                                           desc, false, res);
        if (*res) {
          return false;
        }
        if (!succeeded) {
          REJECT();
        }
        if (index >= old_len) {
          old_len_desc->set_value(static_cast<double>(index+1));
        }
        return true;
      }
    }
    return JSObject::DefineOwnProperty(ctx, name, desc, th, res);
  }
}

#undef REJECT

JSArray* JSArray::New(Context* ctx) {
  JSArray* const ary = new JSArray(ctx, 0);
  const Class& cls = ctx->Cls("Array");
  ary->set_cls(cls.name);
  ary->set_prototype(cls.prototype);
  return ary;
}

} }  // namespace iv::lv5