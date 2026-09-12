// Copyright 2026 Google LLC

/** @file include/kmeans0cfg.h
 Wrapper class to package KMeans related configurations/flags */

#ifndef _INCLUDE_VECTOR0CFG_H_
#define _INCLUDE_VECTOR0CFG_H_

#include <any>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "vector0types.h"

#include "include/mysqld_error.h"
#include "ut0dbg.h"
#include "ut0log.h"

#include "my_rapidjson_size_t.h"
#include "rapidjson/document.h"

namespace ib_vector {

/** Configurable vector Options
@remarks Only add options when they are meaningfully customizable to MySQL
implementation. Or else we'd better off just leave them (default) to vector.
To do so, insert an entry for each (vector) option to add and specialized it
below based on the VectorOption<Enum, Type> template. */
enum class Options {
  INDEX_TYPE,               // the type of vector index used for ANN searches
  DIST_MEASURE,             /*!< the formula used to calc vector distance */
  TREE_LEVELS,              /*!< # of level for the index tree */
  NUM_PARTITIONS,           /*!< # of partitions in the index tree */
  NUM_NEIGHBORS,            /*!< # of neighbors to search for a query */
  SAMPLE_SIZE,              /*!< the (calculated training) sample size */
  CLUSTERS_PER_BLOCK,       /*!< # of clusters (or quantizers) per block */
  MIN_CLUSTER_SIZE,         /*!< min # of clusters */
  MAX_CLUSTERING_ITERATION, /*!< max # of iterations used in clustering */
  VECTOR_DIMENSION,         /*!< dimension of the vector */
  NUM_DIMENSIONS_PER_BLOCK, /*!< # of dimensions per block in chunking */
  NUM_BLOCKS,               /*!< # of blocks in chunking */
  NUM_QUERY_PARTITIONS,     /*!< # of partitions to search in a query */
  NUM_TLP,                  /*!< # of partitions in the top level tree (TLP) */
  NUM_SEARCH_TLP            /*!< # of TLP centroids to search for a query */
};

/** Base class for vector option */
class OptionItem {
 public:
  // no default constructor, have to have a proper option type
  OptionItem() = delete;
  virtual ~OptionItem() = default;

  // operators

  /** Assignment (=) Operator
  @param[in]       other          the content to copy from
  @remarks This is a shallow copy, since down to the deepest layer, the
           OptionItem objects are created immutable and referenced using
           shared pointers. There is no need to do deep copy. */
  OptionItem& operator = (const OptionItem&) = default;

  /** Cast operator
  @return the option value casted to the wanted type */
  template<typename T> operator T () const {
    if (const auto* p = std::any_cast<T>(&m_value)) return *p;

    if constexpr (std::is_enum_v<T>) {
      if (const auto* p = std::any_cast<int>(&m_value)) return static_cast<T>(*p);
      if (const auto* p = std::any_cast<uint8_t>(&m_value)) return static_cast<T>(*p);
      if (const auto* p = std::any_cast<unsigned int>(&m_value)) return static_cast<T>(*p);
    } else if constexpr (std::is_arithmetic_v<T>) {
      if (const auto* p = std::any_cast<float>(&m_value)) return static_cast<T>(*p);
      if (const auto* p = std::any_cast<double>(&m_value)) return static_cast<T>(*p);
      if (const auto* p = std::any_cast<int>(&m_value)) return static_cast<T>(*p);
      if (const auto* p = std::any_cast<unsigned int>(&m_value)) return static_cast<T>(*p);
      if (const auto* p = std::any_cast<size_t>(&m_value)) return static_cast<T>(*p);
    }
    
    throw std::bad_any_cast();
  }

  // getters & settings

  /** Get the option type
  @return the option type */
  Options option() const { return m_option; }

  /** Get the option name
  @return the option name */
  std::string name() const {
    for (auto const & row : m_names) {
      if (row.opt == m_option) {
        return row.name;
      }
    }
    ut_error;
  }

  /** Get the option value
  @return the option value
  @remarks It is defined as a pure virtual function to enforce a specific
           implementation for each option type. */
  virtual std::string value() const = 0;

 protected:
  /** Constructor
  @param[in]        opt           the option type
  @param[in]        val           the option value
  @remarks This is defined as a protected function so OptionItem can only be
           created by the friend (VectorCfg) class. */
  OptionItem(Options opt, std::any val) : m_option(opt), m_value(val) {}

  /** Convert a string value to the option value
  @param[in]        str           the string value to convert
  @return the option value */
  virtual std::any val_from_str(const std::string& str) const = 0;

  /** Convert a string value to the option type
  @param[in]        str           the string value to convert
  @return the option type */
  static Options str_to_option(const std::string& str) {
    for (auto const & row : m_names) {
      if (str == row.name) {
        return row.opt;
      }
    }
    ut_error;
  }

 protected:
  const Options m_option;
  const std::any m_value;

 private:
  static constexpr struct { Options opt; const char* name; } m_names[] = {
  //  Options                               name
    { Options::INDEX_TYPE,                  "index_type" },
    { Options::DIST_MEASURE,                "distance_measure" },
    { Options::TREE_LEVELS,                 "tree_levels" },
    { Options::NUM_PARTITIONS,              "num_partitions" },
    { Options::NUM_NEIGHBORS,               "num_neighbors" },
    { Options::SAMPLE_SIZE,                 "sample_size" },
    { Options::CLUSTERS_PER_BLOCK,          "clusters_per_block" },
    { Options::MIN_CLUSTER_SIZE,            "min_cluster_size" },
    { Options::MAX_CLUSTERING_ITERATION,    "max_clustering_iteration" },
    { Options::VECTOR_DIMENSION,            "vector_dimensions" },
    { Options::NUM_DIMENSIONS_PER_BLOCK,    "num_dimensions_per_block" },
    { Options::NUM_BLOCKS,                  "num_blocks" },
    { Options::NUM_QUERY_PARTITIONS,        "num_query_partitions" },
    { Options::NUM_TLP,                     "num_tlp" },
    { Options::NUM_SEARCH_TLP,              "num_search_tlp" },
  };

  friend class VectorCfg;
};

/** Template class for each vector option */
template<Options opt> class Option;

/** Option<...> Specialization for INDEX_TYPE */
template<> class Option<Options::INDEX_TYPE> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::INDEX_TYPE, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::INDEX_TYPE, val_from_str(val_str)) {}
  virtual std::string value() const override {
    if (const auto* p = std::any_cast<IndexType>(&m_value)) {
      return IndexTypeToString(*p);
    }
    if (const auto* p = std::any_cast<int>(&m_value)) {
      return IndexTypeToString(static_cast<IndexType>(*p));
    }
    if (const auto* p = std::any_cast<uint8_t>(&m_value)) {
      return IndexTypeToString(static_cast<IndexType>(*p));
    }
    throw std::bad_any_cast();
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)StringToIndexType(str);
  }
};

/** Option<...> Specialization for DIST_MEASURE */
template<> class Option<Options::DIST_MEASURE> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::DIST_MEASURE, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::DIST_MEASURE, val_from_str(val_str)) {}
  virtual std::string value() const override {
    return DistMeasureToString(std::any_cast<DistMeasure>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)StringToDistMeasure(str);
  }
};

/** Option<...> Specialization for TREE_LEVELS */
template<> class Option<Options::TREE_LEVELS> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::TREE_LEVELS, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::TREE_LEVELS, val_from_str(val_str)) {}
  virtual std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for NUM_PARTITIONS */
template<> class Option<Options::NUM_PARTITIONS> : public OptionItem {
 public:
  explicit Option(std::any val) : OptionItem(Options::NUM_PARTITIONS, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::NUM_PARTITIONS, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for NUM_NEIGHBORS */
template<> class Option<Options::NUM_NEIGHBORS> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::NUM_NEIGHBORS, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::NUM_NEIGHBORS, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for SAMPLE_SIZE */
template<> class Option<Options::SAMPLE_SIZE> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::SAMPLE_SIZE, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::SAMPLE_SIZE, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for CLUSTERS_PER_BLOCK */
template<> class Option<Options::CLUSTERS_PER_BLOCK> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::CLUSTERS_PER_BLOCK, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::CLUSTERS_PER_BLOCK, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for MIN_CLUSTER_SIZE */
template<> class Option<Options::MIN_CLUSTER_SIZE> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::MIN_CLUSTER_SIZE, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::MIN_CLUSTER_SIZE, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<float>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stof(str);
  }
};

/** Option<...> Specialization for VECTOR_DIMENSION */
template<> class Option<Options::VECTOR_DIMENSION> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::VECTOR_DIMENSION, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::VECTOR_DIMENSION, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for NUM_DIMENSIONS_PER_BLOCK */
template<> class Option<Options::NUM_DIMENSIONS_PER_BLOCK> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::NUM_DIMENSIONS_PER_BLOCK, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::NUM_DIMENSIONS_PER_BLOCK, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for NUM_BLOCKS */
template<> class Option<Options::NUM_BLOCKS> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::NUM_BLOCKS, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::NUM_BLOCKS, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for MAX_CLUSTERING_ITERATION */
template<> class Option<Options::MAX_CLUSTERING_ITERATION> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::MAX_CLUSTERING_ITERATION, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::MAX_CLUSTERING_ITERATION, val_from_str(val_str)) {}
  virtual std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for NUM_QUERY_PARTITIONS */
template<> class Option<Options::NUM_QUERY_PARTITIONS> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::NUM_QUERY_PARTITIONS, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::NUM_QUERY_PARTITIONS, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for NUM_TLP */
template<> class Option<Options::NUM_TLP> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::NUM_TLP, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::NUM_TLP, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Option<...> Specialization for NUM_SEARCH_TLP */
template<> class Option<Options::NUM_SEARCH_TLP> : public OptionItem {
 public:
  Option(std::any val) : OptionItem(Options::NUM_SEARCH_TLP, val) {}
  Option(const std::string& val_str)
      : OptionItem(Options::NUM_SEARCH_TLP, val_from_str(val_str)) {}
  std::string value() const override {
    return std::to_string(std::any_cast<int>(m_value));
  }
 protected:
  virtual std::any val_from_str(const std::string& str) const override {
    return (std::any)std::stoi(str);
  }
};

/** Priority of updates for the vector options
The ordering from lowest to highest is:
    (vector's library default) -- not modeled here to avoid endless src chasing
    DEFAULT                    -- hard-coded defaults to be used
    GENERATED                  -- values calculated internally
    CONFIG                     -- customer explicit configurations */
enum VectorCfgPriority { DEFAULT_SETTING, GENERATED_SETTING, CONFIG_SETTING };

/** Vector Configurations.
This class is to collect and keep track of the vector options, and to generate
native ANN engine (such as KMeans) configurations.

This a per index object, its intended usage looks like:
  VectorCfg cfg;                  // start with factory (hard-coded) defaults
  cfg.set_default(o1, v1);        // set the default value v1 to option o1
  cfg.set_generated(o1, v2);      // set the generated value v2 to o1
  cfg.set_cfg(o1, v3);            // set the customized value v3 to o1

  cfg.set_default(o2, v4);
  cfg.set_generated(o2, v5);

  cfg.set_cfg(o3, v6);
  cfg.set_cfg(o2, v7);

  assert(v3 == cfg[o1]);          // user confuration has the highest priority
  assert(v5 == cfg[o2]);          // generated value overwrites default
  assert(v7 == cfg[o3]);          // setting on the same level means overwrite

  assert(cfg.find(o1) && cfg.find(o2) && cfg.find(o3));
  assert(!cfg.find(o9));

This class also supports json format persistence, used to persist vector index
properties for recreate (db restart) and similar scenarios. For example,
  {
    "num_partitions": {
        "G": 354
    },
    "distance_measurement": {
        "C": "CosineDistance"
    },
    ...
    "index_type": {
        "D": "TREE_SQ",
        "C": "TREE_SQ"
    }
  }

The persisted json string can be parsed back to VectorCfg object using the
constructor that takes a string input, e.g.,
  VectorCfg cfg(json_str);

The persistence and reload is used to recreate the vector index after database
restart.*/
class VectorCfg {
  /** A ledger to keep track of updates, one copy each level (priority). */
  class OptionLedger {
   public:
    OptionLedger() = default;
    OptionLedger(Options option) : m_option(option) {}
    OptionLedger(const OptionLedger& other) { *this = other; }

    /** Assignment (=) Operator
    @param[in]      other         the content to copy from
    @remarks This is a shallow copy, since down to the deepest layer, the
             OptionItem objects are created immutable and referenced using
             shared pointers. There is no need to do deep copy. */
    OptionLedger& operator = (const OptionLedger& other) {
      m_option = other.m_option;
      m_edits.clear();
      if (!other.m_edits.empty()) {
        for (const auto &e : other.m_edits) {
          m_edits[e.first] = e.second;
        }
      }
      return *this;
    }

    /** Compare (==) operator
    @param[in]      other         the other OptionLedger instance to compare to
    @return true if the same
    @remarks This comparison not only compares the final, i.e., top priority
             values, but also all other edits. */
    bool operator == (const OptionLedger& other) const {
      bool equal = (m_option == other.m_option) &&
                   (m_edits.size() == other.m_edits.size());
      if (equal) {
        for (auto const& e : m_edits) {
          if (other.m_edits.at(e.first)->name() != e.second->name() ||
              other.m_edits.at(e.first)->value() != e.second->value()) {
            return false;
          }
        }
      }
      return equal;
    }

    /** Cast operator
    @return the highest priority value to the casted type */
    template<typename T> operator T () const {
      ut_a(!m_edits.empty());
      return (T) *(m_edits.rbegin()->second);
    }

    /** Edit the value of this option
    @param[in]      edit          the new edit of an option (both name & value)
    @param[in]      priority      the layer this edit belongs to */
    void set(std::shared_ptr<OptionItem>edit, VectorCfgPriority priority) {
      ut_a(m_option == edit->option());
      m_edits[priority] = edit;
    }

    /** Check if this option is set at a given priority
    @param[in]      priority      the priority this edit belongs to
    @return true if it is set. */
    bool find(VectorCfgPriority priority) const {
      return m_edits.find(priority) != m_edits.end();
    }

    /** Get the (highest priority) value of this option in string format
    @return the value as a string */
    std::string value() const {
      ut_a(!m_edits.empty());
      return m_edits.rbegin()->second->value();
    }

    /** Output the option to a json string
    @return A string in json format, with overwriting values explained
    @remakrs Sample output: "num_neighbors": { "D": 10, "C": 20 } */
    void to_json_object(rapidjson::Document& doc) const;

    /** Verbose output of the option
    @return A string with overwriting values explained
    @remarks Sample output: min_children_num: 5(D) -> 6(G) -> 7(C) */
    std::string to_verbose_string() const;

    /** Convert a string to a priority level
    @param[in]      str           the string to convert
    @return the priority level */
    static VectorCfgPriority str_to_priority(std::string str) {
      for (auto const & row : m_priority_table) {
        if (str == row.str) {
          return row.priority;
        }
      }
      ut_error;
    }

   protected:
    constexpr static struct { VectorCfgPriority priority; const char* str; }
    m_priority_table [] = {
    //  priority level            string
      { DEFAULT_SETTING,          "D" },
      { GENERATED_SETTING,        "G" },
      { CONFIG_SETTING,           "C" },
    };

   private:
    std::map<VectorCfgPriority, std::shared_ptr<OptionItem>> m_edits;
    Options m_option;
  };

 public:
  /** Constructor, populate the factory defaults first. */
  VectorCfg();

  /** Copy constructor
  @remarks It is required in an lvalue assignment statement */
  VectorCfg(const VectorCfg&) = default;

  /** Move constructor
  @param[in]        other         the cfg object to inherit from
  @remarks Optimzed to avoid too much copying */
  VectorCfg(VectorCfg&& other) { this->m_options.swap(other.m_options); }

  /** Constructor
  @param[in]        str           configuration serialized in json format */
  VectorCfg(const char* str);

  ~VectorCfg() = default;

  bool operator == (const VectorCfg& other) const {
    bool equal = (m_options.size() == other.m_options.size());
    if (equal) {
      for (auto const& opt : m_options) {
        if (opt.second != other.m_options.at(opt.first)) {
          return false;
        }
      }
    }
    return true;
  }

  /** Set a user configured option
  @param[in]        option        the option to set
  @param[in]        value         the value to set for the given option
  @return self (for concatenation syntax) */
  VectorCfg& set_cfg(Options option, std::any value) {
    return set(option, value, CONFIG_SETTING);
  }

  /** Set a default option
  @param[in]        option        the option to set
  @param[in]        value         the value to set for the given option
  @return self (for concatenation syntax) */
  VectorCfg& set_default(Options option, std::any value) {
    return set(option, value, DEFAULT_SETTING);
  }

  /** Set a generated option
  @param[in]        option        the option to set
  @param[in]        value         the value to set for the given option
  @return self (for concatenation syntax) */
  VectorCfg& set_generated(Options option, std::any value) {
    return set(option, value, GENERATED_SETTING);
  }

  /** Check if an option is set
  @param[in]        option        the option to look for
  @return true if the option can be found */
  bool find(Options option) const {
    return m_options.find(option) != m_options.end();
  }

  /** Check if an option is set at a given priority
  @param[in]        option        the option to look for
  @param[in]        priority      priority at which to check
  @return true if the option can be found */
  bool find(Options option, VectorCfgPriority priority) const {
    auto it = m_options.find(option);
    if (it == m_options.end()) {
      return false;
    }
    return it->second.find(priority);
  }

  /** Operator to expose the option value, same as a getter function
  @param[in]        option        whose value to retrieve
  @return a reference to the OptionItem object, which can be (im- & explicitly)
  casted to wanted type.
  @remarks It is not implemented as a function to avoid redundant casts. */
  const OptionLedger& operator [] (Options option) const {
    auto opt = m_options.find(option);
    ut_a(opt != m_options.end());
    return opt->second;
  }

  /** Output the full configurations
  @return a string with the full content (for debugging purpose by design) */
  std::string to_string() const;

  /** Output the full configurations in json format
  @return a string with the full content in json format */
  std::string to_json_string() const;

 protected:
  /** Set an option
  @param[in]        option        the option to set
  @param[in]        value         the value to set for the given option
  @param[in]        priority      the priority of this value
  @return self (for concatenation syntax) */
  VectorCfg& set(Options option, std::any value, VectorCfgPriority priority);

  /** Create an object for option.
  @param[in]        opt           the type of this option
  @param[in]        val           the value of this option
  @return the object wrapped in a shared pointer. */
  static std::shared_ptr<OptionItem> create_option(Options opt, std::any val);

  /** Create an object for option.
  @param[in]        opt_str       the string representation of this option
  @param[in]        val_str       the string representation of this option
  @return the object wrapped in a shared pointer. */
  static std::shared_ptr<OptionItem>
  create_option(const std::string& opt_str, const std::string& val_str);

  /** List of KMeans options, initialized with factory defaults */
  std::map<Options, OptionLedger> m_options;
};

} /* namespace ib_vector */

#endif /* _INCLUDE_VECTOR0CFG_H_ */
