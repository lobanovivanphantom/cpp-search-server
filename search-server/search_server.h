#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <tuple>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <string_view>
#include <execution>
#include "document.h"
#include "string_processing.h"

using namespace std::literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EXP = 1e-6;

class SearchServer {
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);

	explicit SearchServer(const std::string& stop_words_text) : SearchServer(
		SplitIntoWords(stop_words_text)) {}

	explicit SearchServer(std::string_view stop_words_text) : SearchServer(
		SplitIntoWords(stop_words_text)) {}

	void AddDocument(int document_id, std::string_view document, DocumentStatus status,
		const std::vector<int>& ratings);

	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query,
		DocumentPredicate document_predicate) const;

	std::vector<Document> FindTopDocuments(std::string_view raw_query,
		DocumentStatus status = DocumentStatus::ACTUAL) const {
		return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status; });
	}

	int GetDocumentCount() const;

	

	std::set<int>::iterator begin() const;

	std::set<int>::iterator end() const;

	//частоты слова по id документа, eсли документа не существует-пустой map
	const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

	// удаление документов из поискового сервера
	void RemoveDocument(int document_id);

	//многопоточный метод удаления
	template<class Execution>
	void RemoveDocument(Execution&& policy, int document_id);

	// Реализация 3 методов MatchDocument описание в cpp
	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query,
		int document_id) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy&,
		std::string_view raw_query, int document_id) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&,
		std::string_view raw_query, int document_id) const;



private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};
	const std::set<std::string, std::less<>> stop_words_;
	std::map<std::string, std::map<int, double>, std::less<>> word_to_document_;
	std::map<int, std::map<std::string, double>> document_to_word_;
	std::map<int, DocumentData> documents_;
	std::set<int> document_ids_;

	bool IsStopWord(std::string_view word) const;

	static bool IsValidWord(std::string_view word);

	std::vector<std::string> SplitIntoWordsNoStop(std::string_view text) const;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(std::string_view text) const;

	struct Query {
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	Query ParseQuery(std::string_view text, bool flag = false) const;

	double ComputeWordInverseDocumentFreq(std::string_view word) const;

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query& query,
		DocumentPredicate document_predicate) const;
};

template<class Execution>
void SearchServer::RemoveDocument(Execution&& policy, int document_id)
{
	const auto it_word = document_to_word_.find(document_id);
	if (it_word == document_to_word_.end())
		return;
	std::vector<const std::string*> words;
	std::transform(
		it_word->second.begin(),
		it_word->second.end(),
		std::back_inserter(words),
		[](const auto& word) { return &word.first; });
	std::for_each(policy,
		words.begin(), words.end(),
		[&](const auto& word) {
			const auto it = word_to_document_.find(*word);
			if (it != word_to_document_.end()) {
				it->second.erase(document_id);
			}
		});
	document_to_word_.erase(document_id);
	documents_.erase(document_id);
	document_ids_.erase(document_id);
}


template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
	: stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
	if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
		throw std::invalid_argument("Some of stop words are invalid");
	}
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
	DocumentPredicate document_predicate) const
{
	const auto query = ParseQuery(raw_query, true);

	auto matched_documents = FindAllDocuments(query, document_predicate);

	sort(matched_documents.begin(), matched_documents.end(),
		[](const Document& lhs, const Document& rhs) {
			if (std::abs(lhs.relevance - rhs.relevance) < EXP) {
				return lhs.rating > rhs.rating;
			}
			else {
				return lhs.relevance > rhs.relevance;
			}});

	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
	std::map<int, double> document_to_relevance;
	for (const std::string_view& word : query.plus_words) {
		if (word_to_document_.count(word) == 0) {
			continue;
		}
		const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
		for (const auto& [document_id, term_freq] : (*word_to_document_.find(word)).second) {
			const auto& document_data = documents_.at(document_id);
			if (document_predicate(document_id, document_data.status, document_data.rating)) {
				document_to_relevance[document_id] += term_freq * inverse_document_freq;
			}
		}
	}

	for (const std::string_view& word : query.minus_words) {
		if (word_to_document_.count(word) == 0) {
			continue;
		}
		for (const auto& [document_id, _] : (*word_to_document_.find(word)).second) {
			document_to_relevance.erase(document_id);
		}
	}

	std::vector<Document> matched_documents;
	for (const auto& [document_id, relevance] : document_to_relevance) {
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}
	return matched_documents;
}
