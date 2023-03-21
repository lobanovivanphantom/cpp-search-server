#include "search_server.h"
#include "string_processing.h"
#include <numeric>
#include <iostream>
#include <execution>
using namespace std;


// Добавление документов в поисковый индекс (документ не пустой -- айди уникальный и не отрицательный -- текст документа без стоп слов -- обновление словарей -- вычисление рейтинга)
void SearchServer::AddDocument(int document_id, std::string_view document,
	DocumentStatus status, const std::vector<int>& ratings) {
	if (document.empty()) {
		throw invalid_argument("Empty document"s);
	}
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
		throw invalid_argument("Invalid id"s);
	}
	const vector<string> words = SplitIntoWordsNoStop(document);
	const double inv_word_count = 1.0 / words.size();
	for (const string& word : words) {
		word_to_document_[word][document_id] += inv_word_count;
		document_to_word_[document_id][word] += inv_word_count;
	}
	documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
	document_ids_.insert(document_id);
}


// Количество документов добавленный в поисковый индекс
int SearchServer::GetDocumentCount() const {
	return documents_.size();
}


// итератор начала
std::set<int>::iterator SearchServer::begin() const {
	return document_ids_.begin();
}

// итератор конца
std::set<int>::iterator SearchServer::end() const {
	return document_ids_.end();
}

// Частота встречаемости слова 
const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const
{
	//O(log N)O(logN)
	const static std::map<std::string, double> empty_results = {};
	if (document_ids_.count(document_id) == 0) {
		return empty_results;
	}
	return document_to_word_.at(document_id);
}

// функция удаления документа последовательным способом
void SearchServer::RemoveDocument(int document_id)
{
	RemoveDocument(std::execution::seq, document_id);
}


// Функция поиска документов
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query,
	int document_id) const
{
	if (document_id < 0) {
		throw std::invalid_argument("Negative document ID"s);
	}
	if (!documents_.count(document_id)) {
		throw std::invalid_argument("Document with this ID not found"s);
	}
	std::string_view s_raw_query{ raw_query };
	const auto query = ParseQuery(raw_query, true);
	std::vector<std::string_view> matched_words;
	// проверка на минус слова
	for (const std::string_view& word : query.minus_words) {
		if (word_to_document_.count(word) == 0) {
			continue;
		}
		if (word_to_document_.at(std::string(word)).count(document_id)) {
			matched_words.clear();
			return std::tuple(matched_words, documents_.at(document_id).status);
		}
	}
	//проверка на плюс слова
	for (const std::string_view& word : query.plus_words) {
		if (word_to_document_.count(word) == 0) {
			continue;
		}
		if (word_to_document_.at(std::string(word)).count(document_id)) {
			matched_words.push_back(word);
		}
	}

	return std::tuple(matched_words, documents_.at(document_id).status);
}


// Метод MatchDocument с последовательным выполнением (вызов обычной MatchDocument)
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&,
	std::string_view raw_query, int document_id) const
{
	return MatchDocument(raw_query, document_id);
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const {
	if (document_id < 0 || documents_.count(document_id) == 0) {
		throw std::out_of_range("Invalid document_id");
	}
	if (!IsValidWord(raw_query)) {
		throw std::invalid_argument("Invalid query");
	}

	const auto query = ParseQuery(raw_query);

	std::vector<std::string_view> matched_words(query.plus_words.size() + query.minus_words.size());

	if (std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](auto& word) {
		return word_to_document_.at((std::string)word).count(document_id);
		}
	)
		) {
		return { {}, documents_.at(document_id).status };
	}
	std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(),
		[&](std::string_view word) {
			return word_to_document_.at((std::string)word).count(document_id);
		}
	);
	std::sort(std::execution::par, matched_words.begin(), matched_words.end());
	auto word_end = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
	matched_words.erase(word_end, matched_words.end());
	if (matched_words[0].empty()) {
		matched_words.erase(matched_words.begin());
	}
	return { matched_words, documents_.at(document_id).status };
}


// это стоп-слово?
bool SearchServer::IsStopWord(std::string_view word) const
{
	return stop_words_.count(word) > 0;
}

// это валидное слово?
bool SearchServer::IsValidWord(std::string_view word)
{
	// A valid word must not contain special characters
	return none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
		});
}


// парсинг строки без стоп-слов
std::vector<std::string> SearchServer::SplitIntoWordsNoStop(std::string_view text) const
{
	std::vector<std::string> words;
	for (const string_view& word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {
			throw invalid_argument("Word is invalid");
		}
		if (!IsStopWord(word)) {
			std::string temp(word.begin(), word.end());
			words.push_back(temp);
		}
	}
	return words;
}

// вычисление рейтинга
int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
	if (ratings.empty()) {
		return 0;
	}
	int rating_sum = 0;
	for (const int rating : ratings) {
		rating_sum += rating;
	}
	return rating_sum / static_cast<int>(ratings.size());
}


// парсинг строки
SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const
{
	bool is_minus = false;
	std::string_view text_ = text;
	if (text_[0] == '-') {
		is_minus = true;
		text_ = text_.substr(1);
	}
	if (text_.empty())
		throw std::invalid_argument("There is an invalid character in the text minus in the search query!"s);
	if (text_[0] == '-')
		throw std::invalid_argument("There is an invalid character in the text minus in the search query!"s);
	return { text_, is_minus, IsStopWord(text_) };
}

// парсинг строки
SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool flag) const
{
	if (!IsValidWord(text))
		throw invalid_argument("The query contains special characters!");

	Query result;
	for (const string_view& word : SplitIntoWords(text)) {
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			(query_word.is_minus) ? result.minus_words.push_back(query_word.data) : result.plus_words.push_back(query_word.data);
		}
	}
	//сортировка и удаление лишних дубликатов
	if (flag) {
		std::sort(result.minus_words.begin(), result.minus_words.end());
		auto last_min = std::unique(result.minus_words.begin(), result.minus_words.end());
		result.minus_words.erase(last_min, result.minus_words.end());
		std::sort(result.plus_words.begin(), result.plus_words.end());
		auto last_plus = std::unique(result.plus_words.begin(), result.plus_words.end());
		result.plus_words.erase(last_plus, result.plus_words.end());
	}
	return result;
}

// IDF
double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const
{
	return log(GetDocumentCount() * 1.0 / (*word_to_document_.find(word)).second.size());
}