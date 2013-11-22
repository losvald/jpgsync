/*
 * Copyright (C) 2011 Leo Osvald <leo.osvald@gmail.com>
 *
 * This file is part of KSP Library.
 *
 * KSP Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KSP Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cctype>

#include <algorithm>
#include <sstream>

#include "string_utils.hpp"

MakeString::operator std::string() const {
  return buffer_.str();
}

MakeString::operator const char*() const {
  return buffer_.str().c_str();
}

std::string ToLowerCase(const std::string& s) {
  std::string ret(s);
  std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
  return ret;
}

bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
  return ToLowerCase(a) == ToLowerCase(b);
}

bool EndsWithin(
    std::string subject,
    size_t read_count,
    std::vector<char>& bytes,
    std::vector<char>::iterator* match_end) {
  size_t from = std::max((size_t)0,
                         bytes.size() - read_count - subject.length() + 1);
  auto it = std::search(
      bytes.begin() + from, bytes.end(),
      subject.begin(), subject.end());
  if (it != bytes.end()) {
    *match_end = it + subject.size();
    return true;
  }
  return false;
}

std::string ReplaceString(std::string subject, const std::string& search,
                          const std::string& replace) {
  std::size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
  return subject;
}
