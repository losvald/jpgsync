/*
 * Copyright (C) 2011,2013 Leo Osvald <leo.osvald@gmail.com>
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

#ifndef PROGRAM_OPTIONS_HPP_
#define PROGRAM_OPTIONS_HPP_

#include <cassert>
#include <cctype>
#include <cstdlib>

#include <exception>
#include <iostream>
#include <iomanip>
#include <map>
#include <vector>

#include "../src/util/string_utils.hpp"

namespace ProgramOptionsStyle {

enum Value {
  kSimple
};

} // namespace ProgramOptionsStyle

template<ProgramOptionsStyle::Value Style = ProgramOptionsStyle::kSimple>
class ProgramOptions {
  // forward declarations
 protected:
  template<typename T = bool> class Option;
 private:
  struct ValueParser;
  struct OptionBase;
  template<typename T> class WrappedPointer;

 public:
  struct Exception : public std::exception {
    explicit Exception(const std::string& msg) : msg(msg) { }
    virtual ~Exception() throw() { }
    const char* what() const throw() {
      return msg.c_str();
    }
    std::string msg;
  };

  ProgramOptions()
      : help('h', "help", "prints usage info", this),
        verbosity('v', "verbosity", "increases verbosity level",
                  LevelParser::Create, this) {
  }

  friend std::ostream& operator<<(std::ostream& os, const ProgramOptions& po) {
    for (typename OptionList::const_iterator it = po.options_.begin(),
             it_end = po.options_.end(); it != it_end; ++it) {
      os << (*it)->name() << ": " << (*it)->ToString() << std::endl;
    }
    return os;
  }

  void Parse(int* argc, char** argv) {
    for (typename OptionList::iterator it = options_.begin(),
             it_end = options_.end(); it != it_end; ++it) {
      (*it)->clear();
    }
    Set(0U, &verbosity);
    Set(false, &help);

    std::vector<int> arg_inds;
    typedef std::map<OptionBase*, ValueParser*> ParserMap;
    ParserMap parser_map;
    bool moreflags = true;
    for (int from = 1, to; from < *argc; ) {
      char* flag = argv[from++];
      if (!moreflags || flag[0] != '-') {
// #ifdef DEBUG
//         std::cerr << "(parsed option value: " << flag << ")\n";
// #endif
        arg_inds.push_back(from - 1);
        continue;
      }
      bool longflag = (flag[1] == '-');
      if (longflag && flag[2] == 0) {   // if "--"
        moreflags = false;
        continue;
      }

      for (to = from; to < *argc && argv[to][0] != '-'; ++to)
        ;

      OptionBase* o = NULL;
      if (longflag) {
        typename NameMap::const_iterator it = name_map_.find(flag + 2);
        if (it != name_map_.end())
          o = it->second;
      } else if (flag[1] && flag[2] == 0) {
        typename AliasMap::const_iterator it = alias_map_.find(flag[1]);
        if (it != alias_map_.end())
          o = it->second;
      }
      if (o == NULL)
        throw Exception(std::string("Unrecognized option '") + flag + "'");
#ifdef DEBUG
      std::cerr << "(parsed " << from << ' ' << to << " as " <<
          o->name() << ")\n";
#endif
      ValueParser*& p = parser_map[o];
      if (!p)
        p = o->CreateParser();
      p->Parse(argv, to - from, &from);
    }

    for (typename ParserMap::const_iterator it = parser_map.begin(),
             it_end = parser_map.end(); it != it_end; ++it) {
      ValueParser* p = it->second;
      p->SetValue(it->first);
      delete p;
    }

    // update argc and argv
    std::vector<int>::const_iterator arg_ind = arg_inds.begin();
    for (int i = 1; i <= static_cast<int>(arg_inds.size()); ++i) {
#ifdef DEBUG
      std::cerr << "(parsed arg: " << argv[*arg_ind] << ")" << std::endl;
#endif
      char* tmp = argv[i];
      argv[i] = argv[*arg_ind];
      argv[*arg_ind] = tmp;
      ++arg_ind;
    }
    *argc = arg_inds.size() + 1;

    InitDefaults(*argc, argv);
  }

  virtual void PrintUsage(std::ostream& os) const {
    os << "Program options:" << std::endl;
    for (typename OptionList::const_iterator it = options_.begin(),
             it_end = options_.end(); it != it_end; ++it) {
      (*it)->PrintUsage(os);
    }
  }

  // Option<> help;                        // declared at the end
  // Option<unsigned> verbosity;           // declared at the end

 protected:
  template<typename T>
  class Option : public OptionBase {
   public:
    struct Parser : public ValueParser {
     protected:
      void SetValue(OptionBase* o0) const {
        T value;
        GetValue(&value);
        static_cast<Option*>(o0)->val_.set(value);
      }

      virtual void GetValue(T* value) const = 0;
    };

    Option(char alias, const std::string& name,
           const std::string& description,
           ProgramOptions* po)
        : OptionBase(alias, name, description, DefaultParser::Create, po) { }

    Option(char alias, const std::string& name,
           const std::string& description,
           ValueParser* (*parser_factory)(),
           ProgramOptions* po)
        : OptionBase(alias, name, description, parser_factory, po) { }

    Option(const std::string& name,
           const std::string& description,
           ProgramOptions* po)
        : OptionBase(0, name, description, DefaultParser::Create, po) { }

    Option(const std::string& name,
           const std::string& description,
           ValueParser* (*parser_factory)(),
           ProgramOptions* po)
        : OptionBase(0, name, description, parser_factory, po) { }

    inline std::string ToString() const { return val_.ToString(); }

    inline const T& operator()() const { return val_(); }

    inline std::size_t count() const { return val_.is_set(); }

   private:
    struct DefaultParser : public Parser {
      DefaultParser() : done_(false) { }

      void Parse(char** argv, unsigned cnt, int* argind) {
        CheckNotParsed(argv, *argind);
        if (cnt == 0)
          throw Exception(std::string("Missing value for option '") +
                          argv[*argind - 1] + "'");
        ParseToken(argv[(*argind)++], &value_);
        done_ = true;
      }

      static ValueParser* Create() { return new DefaultParser(); }

     protected:
      void GetValue(T* value) const {
        *value = value_;
      }

     private:
      void CheckNotParsed(char** argv, int argind) const {
        if (done_)
          throw Exception(std::string("Option '") + argv[argind - 1] +
                          "' specified multiple times.");
      }

      T value_;
      bool done_;
    };

    inline void set(const void* value) {
      val_.set(*static_cast<const T*>(value));
    }

    inline void clear() { val_.clear(); }

    WrappedPointer<T> val_;
  };

  template<typename T>
  class MultiOption : public OptionBase {
   public:
    typedef typename std::vector<T>::const_iterator ValueIter;

    struct Parser : public ValueParser {
      void SetValue(OptionBase* o0) const {
        GetValues(static_cast<MultiOption*>(o0)->values_);
      }

     protected:
      virtual void GetValues(std::vector<T>* values) const = 0;
    };

    MultiOption(char alias, const std::string& name,
                const std::string& description,
                ProgramOptions* po)
        : OptionBase(alias, name, description, DefaultParser::Create, po) { }

    MultiOption(char alias, const std::string& name,
                const std::string& description,
                ValueParser* (*parser_factory)(),
                ProgramOptions* po)
        : OptionBase(alias, name, description, parser_factory, po) { }

    MultiOption(const std::string& name,
                const std::string& description,
                ProgramOptions* po)
        : OptionBase(0, name, description, DefaultParser::Create, po) { }

    MultiOption(const std::string& name,
                const std::string& description,
                ValueParser* (*parser_factory)(),
                ProgramOptions* po)
        : OptionBase(0, name, description, parser_factory, po) { }

    std::string ToString() const {
      return ::ToString(values_);
    }

    inline std::size_t count() const { return values_.size(); }
    ValueIter values_begin() const { return values_.begin(); }
    ValueIter values_end() const { return values_.end(); }

   private:
    struct DefaultParser : public Parser {
      void Parse(char** argv, unsigned cnt, int* argind) {
        values_.push_back(T());
        ParseToken(argv[*argind], &values_.back());
      }

      static ValueParser* Create() { return new DefaultParser(); }

     protected:
      void GetValues(std::vector<T>* values) const {
        *values = values_;
      }

     private:
      std::vector<T> values_;
    };

    void set(const void* value) {
      values_.push_back(*static_cast<const T*>(value));
    }

    void clear() { values_.clear(); }

    std::vector<T> values_;
  };

  struct LevelParser : public Option<unsigned>::Parser {
    LevelParser() : level_(0U) { }

    void Parse(char** argv, unsigned cnt, int* argind) {
      if (cnt) level_ = atoi(argv[(*argind)++]);
      else ++level_;
    }

    static ValueParser* Create() { return new LevelParser(); }

   protected:
    void GetValue(unsigned* value) const {
      *value = level_;
    }

   private:
    unsigned level_;
  };

  virtual void InitDefaults(int argc, char** argv) { }

  template<typename T>
  static inline void Set(const T& value, OptionBase* o) {
    o->set(&value);
  }

  template<typename T>
  static inline void Set(unsigned index, const T& value, MultiOption<T>* o) {
    o->values_[index] = value;
  }

  template<typename T>
  static void SetIfNot(const T& value, OptionBase* o) {
    if (!o->count())
      Set(value, o);
  }

  template<typename T>
  static inline void SetIfNot(unsigned index, const T& value,
                              MultiOption<T>* o) {
    if (!o->count())
      Set(index, value, o);
  }

  template<typename T>
  static inline void Clear(OptionBase* o) {
    o->clear();
  }

 private:
  typedef std::vector<OptionBase*> OptionList;
  typedef std::map<std::string, OptionBase*> NameMap;
  typedef std::map<char, OptionBase*> AliasMap;

  class OptionBase {
   public:
    OptionBase(char alias, const std::string& name,
               const std::string& description,
               ValueParser* (*parser_factory)(),
               ProgramOptions* po)
        : name_(name),
          alias_(alias),
          desc_(description),
          parser_factory_(parser_factory) {
      CheckAlias(alias_);
      CheckName(name_);
      po->Add(this);
    }

    friend std::ostream& operator<<(std::ostream& os, const OptionBase& o) {
      os << o.ToString();
      return os;
    }

    void PrintUsage(std::ostream& os) const {
      os << "\t--";
      if (alias_)
        os << alias_ << ", ";
      os << name_;
      os << std::endl << "\t\t" << desc_ << std::endl;
    }

   public:
    virtual std::string ToString() const = 0;

    ValueParser* CreateParser() const { return parser_factory_(); }

    inline const std::string& name() const { return name_; }

    inline char alias() const { return alias_; }

    inline const std::string& description() const { return desc_; }

    virtual std::size_t count() const = 0;

   private:
    virtual void set(const void* value) = 0;

    virtual void clear() = 0;

    static void CheckName(const std::string& name) {
      static const std::string kMsgPrefix = "Invalid name: ";
      if (name.empty() || !isalnum(name[0]) || name[name.size() - 1] == '-')
        throw Exception(kMsgPrefix + name);
      for (std::size_t i = name.size(); --i > 0; )
        if (!(name[i] == '-' && name[i - 1] != '-') && !isalnum(name[i]))
          throw Exception(kMsgPrefix + name);
    }

    static void CheckAlias(char alias) {
      if (alias && !isalnum(alias))
        throw Exception(std::string("Illegal alias: ") + alias);
    }

    std::string name_;
    char alias_;
    std::string desc_;
    ValueParser* (*parser_factory_)();

    friend class ProgramOptions;
  };

  struct ValueParser {
    virtual void Parse(char** argv, unsigned cnt, int* argind) = 0;

    virtual void SetValue(OptionBase* o) const = 0;

    template<typename T>
    static void ParseToken(const char* token, T* value) {
      std::istringstream iss(token);
      iss >> *value;
    }
  };

  template<typename T>
  class WrappedPointer {
   public:
    WrappedPointer() : p_(NULL) { }

    WrappedPointer(const T& val) : p_(new T(val)) { }

    WrappedPointer(const WrappedPointer<T>& c)
        : p_(c.is_set() ? new T(c()) : NULL) { }

    ~WrappedPointer() { delete p_; }

    inline WrappedPointer& operator=(WrappedPointer<T> c) {
      T* tmp = c.p_; c.p_ = p_; p_ = tmp;
      return *this;
    }

    inline const T& operator()() const { return *p_; }

    inline std::string ToString() const {
      if (!is_set())
        return "(NULL)";
      return ::ToString((*this)());
    }

    inline bool is_set() const { return p_ != NULL; }

   private:
    inline void set(const T& val) { delete p_; p_ = new T(val); }

    bool set_if_not(const T& val) {
      if (is_set()) return false;
      set(val); return true;
    }

    inline void clear() { delete p_; p_ = NULL; }

    T* p_;

    friend class ProgramOptions;
  };

  void Add(OptionBase* o) {
    options_.push_back(o);
    name_map_[o->name()] = o;
    if (o->alias())
      alias_map_[o->alias()] = o;
  }

  OptionList options_;
  NameMap name_map_;
  AliasMap alias_map_;

 public:
  // these fields must be constructed after the private data structures above
  // because they are being added from OptionBase constructor
  Option<> help;
  Option<unsigned> verbosity;
};

template<>
template<>
inline void ProgramOptions<>::ValueParser::ParseToken(const char* token,
                                                      bool* value) {
  *value = true;
}

template<>
template<>
inline
ProgramOptions<>::Option<>::DefaultParser::DefaultParser() : value_(false) {
}

template<>
template<>
inline
void ProgramOptions<>::Option<>::DefaultParser::Parse(char** argv,
                                                      unsigned cnt,
                                                      int* argind) {
  CheckNotParsed(argv, *argind);
  ParseToken(argv[*argind], &value_);
  done_ = true;
}

template<>
template<>
inline
void ProgramOptions<>::Option<std::vector<unsigned> >::DefaultParser::
Parse(char** argv, unsigned cnt, int* argind) {
  CheckNotParsed(argv, *argind);
  value_.reserve(cnt);
  while (cnt--) {
    value_.push_back(0);
    ParseToken(argv[(*argind)++], &value_.back());
  }
  done_ = true;
}

template<>
template<>
inline
void ProgramOptions<>::MultiOption<std::vector<unsigned> >::DefaultParser::
Parse(char** argv, unsigned cnt, int* argind) {
  values_.push_back(std::vector<unsigned>());
  std::vector<unsigned>& value = values_.back();
  value.reserve(cnt);
  while (cnt--) {
    value.push_back(0);
    ParseToken(argv[(*argind)++], &value.back());
  }
}

#endif /* PROGRAM_OPTIONS_HPP_ */
