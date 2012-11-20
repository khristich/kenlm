#ifndef UTIL_STREAM_CHAIN__
#define UTIL_STREAM_CHAIN__

#include "util/stream/block.hh"
#include "util/scoped.hh"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/thread/thread.hpp>

#include <cstddef>

#include <assert.h>

namespace util {
template <class T> class PCQueue;
namespace stream {

struct ChainConfig {
  std::size_t entry_size;
  // Chain's constructor will make thisa multiple of entry_size. 
  std::size_t block_size;
  std::size_t block_count;
  std::size_t queue_length;
};

class Chain;
// Specifies position in chain for Link constructor.
class ChainPosition {
  public:
    const Chain &GetChain() const { return *chain_; }
  private:
    friend class Chain;
    friend class Link;
    ChainPosition(PCQueue<Block> &in, PCQueue<Block> &out, Chain *chain) 
      : in_(&in), out_(&out), chain_(chain) {}

    PCQueue<Block> *in_, *out_;

    Chain *chain_;
};

class Chain {
  public:
    explicit Chain(const ChainConfig &config);

    ~Chain();

    std::size_t EntrySize() const {
      return config_.entry_size;
    }
    std::size_t BlockSize() const {
      return config_.block_size;
    }

    ChainPosition Between();

    ChainPosition Last();

  private:
    ChainConfig config_;

    scoped_malloc memory_;

    boost::ptr_vector<PCQueue<Block> > queues_;

    bool last_called_;
};

// Create the link in the worker thread using the position token.
class Link {
  public:
    explicit Link(const ChainPosition &position);

    ~Link();

    Block &operator*() { return current_; }
    const Block &operator*() const { return current_; }

    Block *operator->() { return &current_; }
    const Block *operator->() const { return &current_; }

    Link &operator++();

    operator bool() const { return current_; }

  private:
    Block current_;
    PCQueue<Block> *in_, *out_;
};

// Intended usage: make this the last member variable of class Owner.   
class LinkThread {
  public:
    template <class Owner> LinkThread(const ChainPosition &position, Owner *owner)
      : thread_(boost::ref(*this), position, owner) {}

    ~LinkThread() {
      thread_.join();
    }

    template <class Owner> void operator()(const ChainPosition &position, Owner *owner) {
      for (Link link(position); link; ++link) {
        owner->Process(*link);
        if (!link) break;
      }
    }

  private:
    boost::thread thread_;
};

} // namespace stream
} // namespace util

#endif // UTIL_STREAM_CHAIN__