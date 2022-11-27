#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
using namespace std;
const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
  string s;
  getline(cin, s);
  return s;
}

int ReadLineWithNumber() {
  int result;
  cin >> result;
  ReadLine();
  return result;
}

vector<string> SplitIntoWords(const string &text) {
  vector<string> words;
  string word;
  for (const char c : text) {
    if (c == ' ') {
      if (!word.empty()) {
        words.push_back(word);
        word.clear();
      }
    } else {
      word += c;
    }
  }
  if (!word.empty()) {
    words.push_back(word);
  }

  return words;
}

struct Document {
  Document() = default;
  Document(int id, double relevance, int rating)
      : id(id), relevance(relevance), rating(rating) {}
  int id = 0;
  double relevance = 0.0;
  int rating = 0;
};

enum class DocumentStatus {
  ACTUAL,
  IRRELEVANT,
  BANNED,
  REMOVED,
};

class SearchServer {
public:
  explicit SearchServer(string text) : SearchServer(SplitIntoWords(text)) {}

  template <typename StringCollection>
  explicit SearchServer(const StringCollection &stop_words) {
    for (const string &word : stop_words) {
      if (!IsValidWord(word)) {
        throw invalid_argument("There are special characters in stop_words"s);
      }
      stop_words_.insert(word);
    }
  }

  void AddDocument(int document_id, const string &document,
                   DocumentStatus status, const vector<int> &ratings) {
    const vector<string> words = SplitIntoWordsNoStop(document);
    if (documents_.count(document_id) != 0) {
      throw invalid_argument("this ID is already present!"s);
    }
    if (document_id < 0) {
      throw invalid_argument("Negative ID!"s);
    }
    if (!IsValidWord(document)) {
      throw invalid_argument(
          "There are special characters in AddDocument function"s);
    }
    const double inv_word_count = 1.0 / words.size();
    for (const string &word : words) {
      word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id,
                       DocumentData{ComputeAverageRating(ratings), status});
    verified_document_id_.push_back(document_id);
  }

  vector<Document> FindTopDocuments(const string &raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
  }

  vector<Document> FindTopDocuments(const string &raw_query,
                                    DocumentStatus status) const {
    return FindTopDocuments(raw_query,
                            [status](int document_id, DocumentStatus status2,
                                     int rating) { return status == status2; });
  }

  template <typename DocumentPredicat>
  vector<Document> FindTopDocuments(const string &raw_query,
                                    DocumentPredicat document_predicat) const {
    const Query query = ParseQuery(raw_query);
    vector<Document> matched_documents =
        FindAllDocuments(query, document_predicat);
    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
           if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
             return lhs.rating > rhs.rating;
           } else {
             return lhs.relevance > rhs.relevance;
           }
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
      matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
  }

  int GetDocumentCount() const { return documents_.size(); }

  tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query,
                                                      int document_id) const {
    if (document_id < 0) {
      throw invalid_argument("ID is negative!"s);
    }
    const Query query = ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string &word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.push_back(word);
      }
    }
    for (const string &word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.clear();
        break;
      }
    }
    return tuple{matched_words, documents_.at(document_id).status};
  }

  int GetDocumentId(int index) const { return verified_document_id_.at(index); }

private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
  };

  set<string> stop_words_;
  map<string, map<int, double>> word_to_document_freqs_;
  map<int, DocumentData> documents_;
  vector<int> verified_document_id_;

  bool IsStopWord(const string &word) const {
    return stop_words_.count(word) > 0;
  }

  static bool IsValidWord(const string &word) {
    return none_of(word.begin(), word.end(),
                   [](char c) { return c >= '\0' && c < ' '; });
  }

  static bool IsLastPrelastSymbol(const string &query) {
    for (int i = 0; i < query.size(); i++) {
      if (query[i] == '-' && query[i+1] == '-')  {
          return false;       
      }
    }
    return true;
  }

  vector<string> SplitIntoWordsNoStop(const string &text) const {
    vector<string> words;
    for (const string &word : SplitIntoWords(text)) {
      if (!IsStopWord(word)) {
        words.push_back(word);
      }
    }
    return words;
  }

  static int ComputeAverageRating(const vector<int> &ratings) {
    if (ratings.empty()) {
      return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
      rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
  }

  struct QueryWord {
    string data;
    bool is_minus;
    bool is_stop;
  };

  QueryWord ParseQueryWord(string text) const {
    bool is_minus = false;
    if (!IsLastPrelastSymbol(text)) {
      throw invalid_argument(
          "There are special characters in MatchDocument function!"s);
    }
    if (!IsValidWord(text)) {
      throw invalid_argument(
          "There are special characters in MatchDocument function!"s);
    }
    if (text[0] == '-') {
      is_minus = true;
      text = text.substr(1);
    }
    return {text, is_minus, IsStopWord(text)};
  }

  struct Query {
    set<string> plus_words;
    set<string> minus_words;
  };

  Query ParseQuery(const string &text) const {
    Query query;
    for (const string &word : SplitIntoWords(text)) {
      const QueryWord query_word = ParseQueryWord(word);

      if (!query_word.is_stop) {
        if (query_word.is_minus) {
          query.minus_words.insert(query_word.data);
        } else {
          query.plus_words.insert(query_word.data);
        }
      }
    }
    return query;
  }

  double ComputeWordInverseDocumentFreq(const string &word) const {
    return log(GetDocumentCount() * 1.0 /
               word_to_document_freqs_.at(word).size());
  }
  template <typename DocumentPredicat>
  vector<Document> FindAllDocuments(const Query &query,
                                    DocumentPredicat document_predicat) const {
    map<int, double> document_to_relevance;
    for (const string &word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
      for (const auto [document_id, term_freq] :
           word_to_document_freqs_.at(word)) {
        if (document_predicat(document_id, documents_.at(document_id).status,
                              documents_.at(document_id).rating)) {
          document_to_relevance[document_id] +=
              term_freq * inverse_document_freq;
        }
      }
    }
    for (const string &word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
        document_to_relevance.erase(document_id);
      }
      if (word.empty()) {
        continue;
      }
    }
    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
      matched_documents.push_back(
          {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
  }
};

void PrintDocument(const Document &document) {
  cout << "{ "s
       << "document_id = "s << document.id << ", "s
       << "relevance = "s << document.relevance << ", "s
       << "rating = "s << document.rating << " }"s << endl;
}
int main() {
  try {
    SearchServer search_server("и в на"s);
    search_server.GetDocumentId(-1);
    (void)search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                                    DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(1, "пушистый пёс и модный ошейник"s,
                              DocumentStatus::ACTUAL, {1, 2});
    search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s,
                              DocumentStatus::ACTUAL, {1, 2});
    search_server.AddDocument(3, "большой пёс скво\x12рец"s,
                              DocumentStatus::ACTUAL, {1, 3, 2});
    const auto documents = search_server.FindTopDocuments("--пушистый"s);
    for (const Document &document : documents) {
      PrintDocument(document);
    }
  } catch (const invalid_argument &e) {
    cout << e.what() << endl;
  } catch (const out_of_range &e) {
    cout << e.what() << " Invalid index!" << endl;
  }
}