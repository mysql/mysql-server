// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class DocumentNodeConstIterator;
class PropertiesNodeConstIterator;

class DocumentNode
{
public:
  // FIXME: proper sizing
  static constexpr std::size_t MAX_NODE_DEPTH = 100;

  DocumentNode()                                     = default;
  DocumentNode(DocumentNode &&)                      = default;
  DocumentNode(const DocumentNode &)                 = default;
  DocumentNode &operator=(DocumentNode &&)           = default;
  DocumentNode &operator=(const DocumentNode &other) = default;
  virtual ~DocumentNode()                            = default;

  virtual std::string Key() const = 0;

  virtual bool AsBoolean() const        = 0;
  virtual std::size_t AsInteger() const = 0;
  virtual double AsDouble() const       = 0;
  virtual std::string AsString() const  = 0;

  virtual std::unique_ptr<DocumentNode> GetRequiredChildNode(const std::string &name) const = 0;
  virtual std::unique_ptr<DocumentNode> GetChildNode(const std::string &name) const         = 0;

  virtual bool GetRequiredBoolean(const std::string &name) const             = 0;
  virtual bool GetBoolean(const std::string &name, bool default_value) const = 0;

  virtual std::size_t GetRequiredInteger(const std::string &name) const                    = 0;
  virtual std::size_t GetInteger(const std::string &name, std::size_t default_value) const = 0;

  virtual double GetRequiredDouble(const std::string &name) const               = 0;
  virtual double GetDouble(const std::string &name, double default_value) const = 0;

  virtual std::string GetRequiredString(const std::string &name) const  = 0;
  virtual std::string GetString(const std::string &name,
                                const std::string &default_value) const = 0;

  virtual DocumentNodeConstIterator begin() const = 0;
  virtual DocumentNodeConstIterator end() const   = 0;

  virtual std::size_t num_children() const                                = 0;
  virtual std::unique_ptr<DocumentNode> GetChild(std::size_t index) const = 0;

  virtual PropertiesNodeConstIterator begin_properties() const = 0;
  virtual PropertiesNodeConstIterator end_properties() const   = 0;

protected:
  std::string DoSubstitution(const std::string &text) const;
  std::string DoOneSubstitution(const std::string &text) const;

  bool BooleanFromString(const std::string &value) const;
  std::size_t IntegerFromString(const std::string &value) const;
  double DoubleFromString(const std::string &value) const;
};

class DocumentNodeConstIteratorImpl
{
public:
  DocumentNodeConstIteratorImpl()                                                      = default;
  DocumentNodeConstIteratorImpl(DocumentNodeConstIteratorImpl &&)                      = default;
  DocumentNodeConstIteratorImpl(const DocumentNodeConstIteratorImpl &)                 = default;
  DocumentNodeConstIteratorImpl &operator=(DocumentNodeConstIteratorImpl &&)           = default;
  DocumentNodeConstIteratorImpl &operator=(const DocumentNodeConstIteratorImpl &other) = default;
  virtual ~DocumentNodeConstIteratorImpl()                                             = default;

  virtual void Next()                                                = 0;
  virtual std::unique_ptr<DocumentNode> Item() const                 = 0;
  virtual bool Equal(const DocumentNodeConstIteratorImpl *rhs) const = 0;
};

class PropertiesNodeConstIteratorImpl
{
public:
  PropertiesNodeConstIteratorImpl()                                              = default;
  PropertiesNodeConstIteratorImpl(PropertiesNodeConstIteratorImpl &&)            = default;
  PropertiesNodeConstIteratorImpl(const PropertiesNodeConstIteratorImpl &)       = default;
  PropertiesNodeConstIteratorImpl &operator=(PropertiesNodeConstIteratorImpl &&) = default;
  PropertiesNodeConstIteratorImpl &operator=(const PropertiesNodeConstIteratorImpl &other) =
      default;
  virtual ~PropertiesNodeConstIteratorImpl() = default;

  virtual void Next()                                                  = 0;
  virtual std::string Name() const                                     = 0;
  virtual std::unique_ptr<DocumentNode> Value() const                  = 0;
  virtual bool Equal(const PropertiesNodeConstIteratorImpl *rhs) const = 0;
};

class DocumentNodeConstIterator
{
public:
  DocumentNodeConstIterator(std::unique_ptr<DocumentNodeConstIteratorImpl> impl)
      : impl_(std::move(impl))
  {}
  DocumentNodeConstIterator(DocumentNodeConstIterator &&)                      = default;
  DocumentNodeConstIterator(const DocumentNodeConstIterator &)                 = delete;
  DocumentNodeConstIterator &operator=(DocumentNodeConstIterator &&)           = default;
  DocumentNodeConstIterator &operator=(const DocumentNodeConstIterator &other) = delete;
  ~DocumentNodeConstIterator()                                                 = default;

  bool operator==(const DocumentNodeConstIterator &rhs) const
  {
    return (impl_->Equal(rhs.impl_.get()));
  }

  bool operator!=(const DocumentNodeConstIterator &rhs) const
  {
    return (!impl_->Equal(rhs.impl_.get()));
  }

  std::unique_ptr<DocumentNode> operator*() const { return impl_->Item(); }

  DocumentNodeConstIterator &operator++()
  {
    impl_->Next();
    return *this;
  }

private:
  std::unique_ptr<DocumentNodeConstIteratorImpl> impl_;
};

class PropertiesNodeConstIterator
{
public:
  PropertiesNodeConstIterator(std::unique_ptr<PropertiesNodeConstIteratorImpl> impl)
      : impl_(std::move(impl))
  {}
  PropertiesNodeConstIterator(PropertiesNodeConstIterator &&)                      = default;
  PropertiesNodeConstIterator(const PropertiesNodeConstIterator &)                 = delete;
  PropertiesNodeConstIterator &operator=(PropertiesNodeConstIterator &&)           = default;
  PropertiesNodeConstIterator &operator=(const PropertiesNodeConstIterator &other) = delete;
  ~PropertiesNodeConstIterator()                                                   = default;

  bool operator==(const PropertiesNodeConstIterator &rhs) const
  {
    return (impl_->Equal(rhs.impl_.get()));
  }

  bool operator!=(const PropertiesNodeConstIterator &rhs) const
  {
    return (!impl_->Equal(rhs.impl_.get()));
  }

  std::string Name() const { return impl_->Name(); }
  std::unique_ptr<DocumentNode> Value() const { return impl_->Value(); }

  PropertiesNodeConstIterator &operator++()
  {
    impl_->Next();
    return *this;
  }

private:
  std::unique_ptr<PropertiesNodeConstIteratorImpl> impl_;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
