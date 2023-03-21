#pragma once

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <execution>
#include <atomic>

const double EPSILON = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const int NUM_BASKET = 12;
class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    void RemoveDocument(int document_id);
    template< class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);

    template <class ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::vector<int>::iterator begin();
    std::vector<int>::iterator end();

    template< class ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy&& policy, std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;
    std::map<std::string_view, double> GetWordFrequencies(int document_id) const;
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;

    };
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_;
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;

    bool IsStopWord(const std::string_view& word) const;
    static bool IsValidWord(const std::string_view& word);
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view& text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWordView {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWordView ParseQueryWord(std::string_view& text) const;

    struct QueryView {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    QueryView ParseQuery(std::string_view text) const;
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <class ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, QueryView& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        using namespace std;
        throw std::invalid_argument("Stop words invalid"s);
    }
}

template< class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    std::map<std::string_view, double> word_freq = GetWordFrequencies(document_id);
    std::vector<std::pair<std::string_view, double>> word_freq_vec(word_freq.begin(), word_freq.end());
    for_each(policy, word_freq_vec.begin(), word_freq_vec.end(), [this, document_id](auto& pair) {
        word_to_document_freqs_.find(pair.first)->second.erase(document_id);
        });

    auto remove_it = find(policy, document_ids_.begin(), document_ids_.end(), document_id);
    if (remove_it != document_ids_.end()) {
        document_ids_.erase(remove_it);
    }

    auto remove_doc_it = documents_.find(document_id);
    if (remove_doc_it != documents_.end()) {
        documents_.erase(remove_doc_it);
    }
}

template <class ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query,
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template< class ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(ExecutionPolicy&& policy, std::string_view raw_query, int document_id) const {
    auto query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;
    std::map<std::string_view, double> word_freq = GetWordFrequencies(document_id);

    for (std::string_view word : query.minus_words) {
        if (word_freq.count(word) > 0) {
            return { matched_words, documents_.at(document_id).status };
        }
    }
    std::vector<std::string_view> plus_words(query.plus_words.begin(), query.plus_words.end());
    matched_words = CopyIfUnordered(policy, plus_words, [&word_freq](const std::string_view& word) { return word_freq.count(word) > 0; });
    return { matched_words, documents_.at(document_id).status };
}

template <class ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, QueryView& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance_concurrent(NUM_BASKET);
    std::vector<std::string_view> plus_words(query.plus_words.begin(), query.plus_words.end());
    for_each(policy, plus_words.begin(), plus_words.end(), [this, &document_predicate, &document_to_relevance_concurrent](std::string_view word) {
        if (word_to_document_freqs_.count(word) > 0) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.find(word)->second) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance_concurrent[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
        });
    std::map<int, double> document_to_relevance = document_to_relevance_concurrent.BuildOrdinaryMap();

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        for (const auto [document_id, _] : word_to_document_freqs_.find(word)->second) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <class ExecutionPolicy, typename Container, typename Predicate>
std::vector<typename Container::value_type> CopyIfUnordered(ExecutionPolicy&& policy, const Container& container, Predicate predicate) {
    std::vector<typename Container::value_type> result(container.size());
    std::atomic_int size = 0;
    for_each(
        policy,
        container.begin(), container.end(),
        [predicate, &size, &result](const auto& value) {
            if (predicate(value)) {
                result[size++] = value;
            }
        }
    );
    result.resize(size);
    return result;
}