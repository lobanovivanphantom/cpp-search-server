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

struct Document {
  int id;
  double relevance;
};
string ReadLine() {
  string s;
  getline(cin, s);
  return s;
}

int ReadLineWithNumber() {
  int result = 0;
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

class SearchServer {
public:
  void SetStopWords(const string &text) {
    for (const string &word : SplitIntoWords(text)) {
      stop_words_.insert(word);
    }
  }

  void AddDocument(int document_id, const string &document) {
    ++document_count_;
    const vector<string> words = SplitIntoWordsNoStop(document);
    double section = 1.0 / words.size();
    for (const string &word : words) {
      word_to_documents_frequent[word][document_id] += section;
    }
  }

  vector<Document> FindTopDocuments(const string &raw_query) const {
    const Query query_words = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query_words);
    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
           return lhs.relevance > rhs.relevance;
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
      matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
  }

private:
  struct Query {
    set<string> plus_words;
    set<string> minus_words;
  };
private:
  map<string, set<int>> word_to_documents_;
  map<string, map<int, double>> word_to_documents_frequent;
  set<string> stop_words_;
  int document_count_ = 0;

  bool IsStopWord(const string &word) const {
    return stop_words_.count(word) > 0;
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

  Query ParseQuery(const string &text) const {
    Query query_words;
    for (const string &word : SplitIntoWordsNoStop(text)) {
      if (word[0] == '-') {
        query_words.minus_words.insert(word.substr(1));
      } else {
        query_words.plus_words.insert(word);
      }
    }
    return query_words;
  }

  double IDFCalc(const string &word) const {
    return log(static_cast<double>(document_count_) /
               word_to_documents_frequent.at(word).size());
  }

  vector<Document> FindAllDocuments(const Query &query_words) const {
    map<int, double> document_to_relevance;
    for (const string &word : query_words.plus_words) {
      if (word_to_documents_frequent.count(word) != 0) {
        double word_IDF = IDFCalc(word);
        map<int, double> id_tf = word_to_documents_frequent.at(word);
        for (const auto &[doc_id, tf] : word_to_documents_frequent.at(word)) {
          document_to_relevance[doc_id] += word_IDF * tf;
        }
      }
    }
    for (const string &word : query_words.minus_words) {
      if (word_to_documents_frequent.count(word) != 0) {
        for (const auto &[doc_id, tf] : word_to_documents_frequent.at(word)) {
          document_to_relevance.erase(doc_id);
        }
      }
    }

    vector<Document> doc_to_relevance;
    for (auto [id, rel] : document_to_relevance) {
      doc_to_relevance.push_back({id, document_to_relevance.at(id)});
    }
    return doc_to_relevance;
  }
};

SearchServer CreateSearchServer() {
  SearchServer search_server;
  search_server.SetStopWords(ReadLine());
  const int document_count = ReadLineWithNumber();
  for (int document_id = 0; document_id < document_count; ++document_id) {
    search_server.AddDocument(document_id, ReadLine());
  }

  return search_server;
}

int main() {
  const SearchServer search_server = CreateSearchServer();
  const string query = ReadLine();
  for (const auto &[document_id, relevance] :
       search_server.FindTopDocuments(query)) {
    cout << "{ document_id = "s << document_id << ", "
         << "relevance = "s << relevance << " }"s << endl;
  }
}