#include "search_server.h"
#include <cmath>

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) { // Invoke delegating constructor from string container

}
SearchServer::SearchServer(std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) { // Invoke delegating constructor from string container

}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }

    const auto words = SplitIntoWordsNoStop(document);
    size_t document_size = words.size();
    const double inv_word_count = 1.0 / document_size;
    for (std::string_view word_view : words) {
        std::string word{ word_view.data(), word_view.size() };
        word_to_document_freqs_[std::string(word)][document_id] += inv_word_count;
        document_to_word_[document_id][word_to_document_freqs_.find(word)->first] += 1.0 / document_size;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.push_back(document_id);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::vector<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

std::vector<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

bool SearchServer::IsStopWord(const std::string_view& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view& word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view& text) const {
    std::vector<std::string_view> words;
    for (const std::string_view& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
        }

        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }

    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWordView SearchServer::ParseQueryWord(std::string_view& text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }

    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }

    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + std::string(text) + " is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.find(word)->second.size());
}

std::map<std::string_view, double> SearchServer::GetWordFrequencies(int document_id) const {
    std::map<std::string_view, double> map = {};
    if (std::count(document_ids_.begin(), document_ids_.end(), document_id) == 0) {
        return map;
    }
    return document_to_word_.at(document_id);
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}

SearchServer::QueryView SearchServer::ParseQuery(std::string_view text) const {
    QueryView result;
    std::vector<std::string_view> splited_words = SplitIntoWords(text);
    for (std::string_view word : splited_words) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }

    return result;
}