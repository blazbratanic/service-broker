#ifndef NONCOPYABLE_HPP
#define NONCOPYABLE_HPP

struct noncopyable {
 protected:
  noncopyable() = default;
  ~noncopyable() = default;

 private:
  noncopyable& operator=(noncopyable const&);
  noncopyable(noncopyable const&);
};

#endif
