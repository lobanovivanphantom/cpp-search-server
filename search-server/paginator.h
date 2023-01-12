template <typename Iterator> class IteratorRange {
public:
  IteratorRange(Iterator begin, Iterator end)
      : beginning(begin), ending(end),
        size_distance(distance(beginning, ending)) {}

  Iterator begin() const { return beginning; }

  Iterator end() const { return ending; }

  size_t size() const { return size_distance; }

private:
  Iterator beginning, ending;
  size_t size_distance;
};

template <typename It>
std::ostream &operator<<(std::ostream &out, const IteratorRange<It> &range) {
  for (It it = range.begin(); it != range.end(); ++it) {
    out << *it;
  }
  return out;
}

template <typename It> class Paginator {
public:
  Paginator(It begin, It end, size_t page_size) {
    for (size_t left = distance(begin, end); left > 0;) {
      const size_t current_page_size = std::min(page_size, left);
      const It current_page_end = next(begin, current_page_size);
      pages_.push_back({begin, current_page_end});

      left -= current_page_size;
      begin = current_page_end;
    }
  }

  auto begin() const { return pages_.begin(); }

  auto end() const { return pages_.end(); }

  size_t size() const { return pages_.size(); }

private:
  std::vector<IteratorRange<It>> pages_;
};
template <typename Container>
auto Paginate(const Container &c, size_t page_size) {
  return Paginator(begin(c), end(c), page_size);
}