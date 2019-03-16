#pragma once

#include <optional>
#include <string>
#include <vector>

#include <tinyxml2.h>

#include "dvc/json.h"

namespace relaxng {

using Element = const tinyxml2::XMLElement*;
using Attribute = const tinyxml2::XMLAttribute*;

enum class MemberKind { ATTRIBUTE, SUBELEMENT };
enum class MemberDisposition { OPTIONAL, REQUIRED, MULTIPLE };

struct GeneratedClass {
  Element _element_;
  bool _parsed_ = false;
};

template <class Protocol>
struct ProtocolReflection;

template <class Protocol, size_t class_index>
struct ProtocolClassReflection;

template <class Class>
struct ClassReflection;

template <class Class, size_t member_index>
struct ClassMemberReflection;

inline void set_member(std::optional<std::string>& t, std::string_view value) {
  t = value;
}

inline void set_member(std::string& t, std::string_view value) { t = value; }

inline void set_member(std::vector<std::string>& t, std::string_view value) {
  t.push_back(std::string(value));
}

template <class Class, size_t member_index>
void apply_attribute_i(Class& object, Attribute attribute, bool& found) {
  using m = ClassMemberReflection<Class, member_index>;
  if constexpr (m::member_kind == MemberKind::ATTRIBUTE) {
    if (m::input_name == attribute->Name()) {
      found = true;
      set_member(object.*m::member_ptr, attribute->Value());
    }
  }
}

template <class Class, size_t... I>
void apply_attribute(Class& object, Attribute attribute,
                     std::index_sequence<I...>) {
  bool found = false;
  (apply_attribute_i<Class, I>(object, attribute, found), ...);
  DVC_ASSERT(found, attribute->Name(), " line ", attribute->GetLineNum());
}

template <typename T>
struct remove_memptr;
template <class C, typename T>
struct remove_memptr<T C::*const> {
  using type = T;
};
template <typename T>
using remove_memptr_t = typename remove_memptr<T>::type;

template <typename T>
struct remove_disposition {
  using type = T;
  static constexpr auto disposition = MemberDisposition::REQUIRED;
};
template <typename T>
struct remove_disposition<std::optional<T>> {
  using type = T;
  static constexpr auto disposition = MemberDisposition::OPTIONAL;
};
template <typename T>
struct remove_disposition<std::vector<T>> {
  using type = T;
  static constexpr auto disposition = MemberDisposition::MULTIPLE;
};

template <class Class>
Class parse(Element element);

template <class Class, size_t member_index>
void apply_subelement_i(Class& object, Element subelement) {
  using m = ClassMemberReflection<Class, member_index>;
  if constexpr (m::member_kind == MemberKind::SUBELEMENT) {
    if (m::input_name == subelement->Name()) {
      using T = remove_memptr_t<decltype(m::member_ptr)>;
      if constexpr (std::is_same_v<T, std::string> ||
                    std::is_same_v<T, std::optional<std::string>> ||
                    std::is_same_v<T, std::vector<std::string>>)
        set_member(object.*m::member_ptr, subelement->GetText());
      else {
        using rd = remove_disposition<T>;
        using SubelementClass = typename rd::type;
        if constexpr (rd::disposition == MemberDisposition::REQUIRED) {
          DVC_ASSERT(!(object.*m::member_ptr)._parsed_,
                     "required member already present ", subelement->Name(),
                     " line ", subelement->GetLineNum());
          object.*m::member_ptr = parse<SubelementClass>(subelement);
        } else if constexpr (rd::disposition == MemberDisposition::OPTIONAL) {
          DVC_ASSERT(!(object.*m::member_ptr),
                     "optional member already present ", subelement->Name(),
                     " line ", subelement->GetLineNum());
          object.*m::member_ptr = parse<SubelementClass>(subelement);
        } else if constexpr (rd::disposition == MemberDisposition::MULTIPLE) {
          (object.*m::member_ptr).push_back(parse<SubelementClass>(subelement));
        }
      }
    }
  }
}

template <class Class, size_t... I>
void apply_subelement(Class& object, Element subelement,
                      std::index_sequence<I...>) {
  (void)subelement;
  (apply_subelement_i<Class, I>(object, subelement), ...);
}

template <class Class>
Class parse(Element element) {
  Class object;
  object._element_ = element;
  object._parsed_ = true;

  using iseq = std::make_index_sequence<ClassReflection<Class>::num_members>;

  for (Attribute attribute = element->FirstAttribute(); attribute != nullptr;
       attribute = attribute->Next())
    apply_attribute(object, attribute, iseq());

  for (Element subelement = element->FirstChildElement(); subelement != nullptr;
       subelement = subelement->NextSiblingElement()) {
    apply_subelement(object, subelement, iseq());
  }
  return object;
}

template <class Class, size_t member_index>
void write_json_i(dvc::json_writer& w, const Class& object) {
  using m = ClassMemberReflection<Class, member_index>;
  using T = remove_memptr_t<decltype(m::member_ptr)>;
  const auto& value = object.*m::member_ptr;
  if constexpr (std::is_same_v<T, std::string>) {
    w.write_key(m::output_name);
    w.write_string(value);
  } else if constexpr (std::is_same_v<T, std::optional<std::string>>) {
    if (value) {
      w.write_key(m::output_name);
      w.write_string(*value);
    }
  } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
    if (!value.empty()) {
      w.write_key(m::output_name);
      w.start_array();
      for (const auto& v : value) w.write_string(v);
      w.end_array();
    }
  } else {
    using rd = remove_disposition<T>;
    if constexpr (rd::disposition == MemberDisposition::REQUIRED) {
      w.write_key(m::output_name);
      write_json(w, value);
    } else if constexpr (rd::disposition == MemberDisposition::OPTIONAL) {
      if (value) {
        w.write_key(m::output_name);
        write_json(w, *value);
      }
    } else if constexpr (rd::disposition == MemberDisposition::MULTIPLE) {
      w.write_key(m::output_name);
      w.start_array();
      for (const auto& v : value) write_json(w, v);
      w.end_array();
    }
  }
}

template <class Class, size_t... I>
void write_json(dvc::json_writer& w, const Class& object,
                std::index_sequence<I...>) {
  (write_json_i<Class, I>(w, object), ...);
}

template <class Class>
void write_json(dvc::json_writer& w, const Class& object) {
  w.start_object();
  using iseq = std::make_index_sequence<ClassReflection<Class>::num_members>;

  write_json(w, object, iseq());

  w.end_object();
}

}  // namespace relaxng
