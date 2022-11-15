#include <algorithm> 
#include <cmath> 
#include <iostream> 
#include <map> 
#include <set> 
#include <string> 
#include <utility> 
#include <vector> 
#include <numeric> 
using namespace std; 
const int MAX_RESULT_DOCUMENT_COUNT = 5; 
const double EPSILON = 1e-6; 
//#include "search_server.h"
 
/*
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
  
vector<string> SplitIntoWords(const string& text) { 
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
*/

struct Document {
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
 
 vector<string> SplitIntoWords(const string& text) { 
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
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }    
 
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, 
            DocumentData{
                ComputeAverageRating(ratings), 
                status
            });
    }
 
 vector<Document> FindTopDocuments(const string& raw_query) const {
 return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }
 
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status]([[maybe_unused]]int document_id, DocumentStatus status2, [[maybe_unused]]int rating)
            {return  status == status2;});
    }  
 
template <typename DocumentPredicat>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicat document_predicat) const {            
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicat);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                    return lhs.rating > rhs.rating;
                }
                    return lhs.relevance > rhs.relevance;
 
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
 
 
    int GetDocumentCount() const {
        return documents_.size();
    }
 
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }
 
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
 
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;  
 
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
 
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
 
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(),ratings.end(),0);
        return rating_sum / static_cast<int>(ratings.size());
    }
 
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
 
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
 
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
 
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
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
 
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }
    template <typename DocumentPredicat>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicat document_predicat) const {
    map<int, double> document_to_relevance;
    for (const string& word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) { continue;}
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (document_predicat(document_id, documents_.at(document_id).status, documents_.at(document_id).rating) ) 
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }       
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};


/*
void PrintDocument(const Document& document) { 
    cout << "{ "s 
         << "document_id = "s << document.id << ", "s 
         << "relevance = "s << document.relevance << ", "s 
         << "rating = "s << document.rating 
         << " }"s << endl; 
} 
  
int main() { 
    SearchServer search_server; 
    search_server.SetStopWords("и в на"s); 
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3}); 
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7}); 
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1}); 
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9}); 
    cout << "ACTUAL by default:"s << endl; 
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) { 
        PrintDocument(document); 
    } 
    cout << "BANNED:"s << endl; 
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) { 
        PrintDocument(document); 
    } 
    cout << "Even ids:"s << endl; 
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) { 
        PrintDocument(document); 
    } 
    return 0; 
}  
*/

#define RUN_TEST(func) RunTestImpl (func, #func)
 
template <typename T>
void RunTestImpl(const T& t, const string& func_name) {
    t();
    cerr << func_name << " OK"s << endl;
}
 
void Test1() {
}
 
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}
 
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
 
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
 
void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}
 
template <typename print>
ostream& Print(ostream& out, const print& container){
    bool isfirst=1;
    for (const auto& element : container) {
    if (isfirst)  out << ""s;
    else out << ", "s;
    isfirst=0;
    out << element << ""s;
    } 
  return out;
}
 
template <typename invect>
ostream& operator<<(ostream& out, const vector<invect>& container) {
out << "["s;
Print(out, container);
out << "]"s;
return out;
}
 
template <typename inset>
ostream& operator<<(ostream& out, const set<inset>& container) {
out << "{"s;
Print(out, container);
out << "}"s;
    return out;    
}
 
template<typename Tfirst, typename Tsecond>
ostream& operator<<(ostream& out, const pair<Tfirst, Tsecond>& container){
return out<<""<<container.first<<": "<<container.second<<"";
}
 
template<typename Tkey, typename Tvalue>
ostream& operator<<(ostream& out, const map<Tkey, Tvalue>& container){
out<<"{";
Print(out, container);
out<<"}";
return out;
}
 
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
 
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
 
vector<int> TakeEvens(const vector<int>& numbers) {
    vector<int> evens;
    for (int x : numbers) {
        if (x % 2 == 0) {
            evens.push_back(x);
        }
    }
    return evens;
}
 
 
// -------- Начало модульных тестов поисковой системы ----------
 
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов

    // Не забудьте вызывать остальные тесты здесь
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(),1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id,doc_id);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}
 
 
 
void TestAddingDocument () {
    const int document_id = 42;
    const string document_info = "lion is a beautiful animal in Africa"s;
    const vector<int> document_ratings = {1, 2, 3};
    {
        SearchServer server;
        ASSERT(server.FindTopDocuments("is"s).empty());
    }
    {
        SearchServer server;
        server.SetStopWords("is a in"s);
        server.AddDocument(document_id, document_info, DocumentStatus::ACTUAL, document_ratings);
        const auto found_docs = server.FindTopDocuments("lion"s);
        ASSERT_EQUAL(found_docs.size(),1);
    }
    {
        SearchServer server;
        server.AddDocument(document_id, document_info, DocumentStatus::ACTUAL, document_ratings);
        const auto found_docs = server.FindTopDocuments("bear"s);
        ASSERT(found_docs.empty());
    }
    {
        SearchServer server;
        server.AddDocument(document_id,  "   ", DocumentStatus::ACTUAL, document_ratings);
        const auto found_docs = server.FindTopDocuments("                     bear"s);
        ASSERT(found_docs.empty());
    }
    {
        SearchServer server;
        server.AddDocument(document_id,  "", DocumentStatus::ACTUAL, document_ratings);
        const auto found_docs = server.FindTopDocuments("bear"s);
        ASSERT(found_docs.empty());
    }
}
 
 
void TestExcludeStopWordsFromAddingDocumentInfo() {
    const int document_id = 5;
    const string document_info = "lion is a beautiful animal in Africa"s;
    const vector<int> document_ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(document_id, document_info, DocumentStatus::ACTUAL, document_ratings);
        const auto found_docs = server.FindTopDocuments("is"s);
        ASSERT_EQUAL(found_docs.size(),1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id,document_id);
    }
    {
        SearchServer server;
        server.SetStopWords("is a"s);
        server.AddDocument(document_id, document_info, DocumentStatus::ACTUAL, document_ratings);
        ASSERT(server.FindTopDocuments("is"s).empty());
    }
}
 
 
void TestExcludeMinusWordsFromAddedDocumentContent() {
    const int document_id = 10;
    const string document_info = "lion is a beautiful animal in Africa"s;
    const vector<int> document_ratings = {1, 1, 4};
    {
        SearchServer server;
        server.SetStopWords("is a in"s);
        server.AddDocument(document_id, document_info, DocumentStatus::ACTUAL, document_ratings);
        const auto found_docs = server.FindTopDocuments("lion"s);
        ASSERT_EQUAL(found_docs.size(),1);
    }
    {
        SearchServer server;
        server.SetStopWords("is a in"s);
        server.AddDocument(document_id, document_info, DocumentStatus::ACTUAL, document_ratings);
        ASSERT(server.FindTopDocuments("is a in -beautiful"s).empty());
    }
}
 
void TestReverseSortFoundDocumentsRelevance() {
	const int document_id_1 = 1;
	const string document_info_1 = "lion in the Africa"s;
	const vector<int> document_rating_1 = {1, 1, 4};
	const int document_id_2 = 2;
	const string document_info_2 = "lion is a beautiful animal in the Africa"s;
	const vector<int> document_rating_2 = {2, 7, 6};
	const int document_id_3 = 3;
	const string document_info_3 = "big lion in the Africa"s;
	const vector<int> document_rating_3 = {0, 6, 2};
    {
        SearchServer server;
        server.AddDocument(document_id_1, document_info_1, DocumentStatus::ACTUAL, document_rating_1);
        server.AddDocument(document_id_2, document_info_2, DocumentStatus::ACTUAL, document_rating_2);
        server.AddDocument(document_id_3, document_info_3, DocumentStatus::ACTUAL, document_rating_3);
        const auto found_docs = server.FindTopDocuments("lion Africa"s);
        ASSERT_EQUAL(found_docs.size(),3);
        ASSERT_EQUAL(found_docs[2].id,document_id_3);
        ASSERT_EQUAL(found_docs[1].id,document_id_1);
       
    }
    {
        SearchServer server;
        server.AddDocument(document_id_1, document_info_3, DocumentStatus::ACTUAL, document_rating_1);
        server.AddDocument(document_id_2, document_info_2, DocumentStatus::ACTUAL, document_rating_2);
        server.AddDocument(document_id_3, document_info_1, DocumentStatus::ACTUAL, document_rating_3);
        const auto found_docs = server.FindTopDocuments("lion"s);
        ASSERT_EQUAL(found_docs.size(),3);
        ASSERT_EQUAL(found_docs[0].id,document_id_2);
        ASSERT_EQUAL(found_docs[1].id,document_id_1);
        ASSERT_EQUAL(found_docs[2].id,document_id_3);
    }
}
 
 
void TestDocumentRatingCalculationAverage() {
    const int document_id = 20;
    const string document_info = "lion in the Africa"s;
    const vector<int> document_ratings = {5, 1, 20, 3};
    {
        SearchServer server;
        server.AddDocument(document_id, document_info, DocumentStatus::ACTUAL, document_ratings);
        const auto found_docs = server.FindTopDocuments("lion good animal"s);
        ASSERT_EQUAL(found_docs[0].rating,((int)(5+1+20+3+0.0)/4));
    }
}
 
void TestFilteringSearchResultsUserSpecifiedPredicate() {
    const int document_id_1 = 0;
	const string document_info_1 = "cat in the Moscow"s;
	const vector<int> document_rating_1 = {8, -3};
	const int document_id_2 = 1;
	const string document_info_2 = "cat in the big city"s;
	const vector<int> document_rating_2 = {7, 2, 7};
	const int document_id_3 = 2;
	const string document_info_3 = "big cat in the big city"s;
	const vector<int> document_rating_3 = {5, -12, 2, 1};
 
    {
        SearchServer server;
        server.AddDocument(document_id_1, document_info_1, DocumentStatus::ACTUAL, document_rating_1);
        server.AddDocument(document_id_2, document_info_2, DocumentStatus::ACTUAL, document_rating_2);
        server.AddDocument(document_id_3, document_info_3, DocumentStatus::ACTUAL, document_rating_3);
        const auto found_docs = server.FindTopDocuments("cat in big city"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT_EQUAL(found_docs.size(),2);
        ASSERT_EQUAL(found_docs[0].id,document_id_3);
        ASSERT_EQUAL(found_docs[1].id,document_id_1);
    }
 
    {
        SearchServer server;
        server.AddDocument(document_id_1, document_info_1, DocumentStatus::ACTUAL, document_rating_1);
        server.AddDocument(document_id_2, document_info_2, DocumentStatus::REMOVED, document_rating_2);
        server.AddDocument(document_id_3, document_info_3, DocumentStatus::ACTUAL, document_rating_3);
        const auto found_docs = server.FindTopDocuments("cat in big city"s, [](int document_id, DocumentStatus status, int rating) {return status == DocumentStatus::REMOVED;});
        ASSERT_EQUAL(found_docs[0].id, document_id_2);
    }
}
 
void TestSearchDocumentsWithGivenStatus() {
    const int document_id = 42;
    const string document_info = "cat in the city"s;
    const vector<int> document_ratings = {1, 2, 3};
    const DocumentStatus document_status = DocumentStatus::BANNED;
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(document_id, document_info, document_status, document_ratings);
        const auto found_docs = server.FindTopDocuments("beautiful cat in the town"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs.empty());
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(document_id, document_info, DocumentStatus::BANNED, document_ratings);
        const auto found_docs = server.FindTopDocuments("beautiful cat in the town"s, DocumentStatus::BANNED);
        ASSERT(!found_docs.empty());
        ASSERT_EQUAL(found_docs[0].id,document_id);
    }
}
 
void TestCorrectCalculationRelevanceFoundDocuments() {
	const int document_id_1 = 0;
	const string document_info_1 = "lion animal from Africa"s;
	const vector<int> document_rating_1 = {6, -1};
	const int document_id_2 = 1;
	const string document_info_2 = "hyppopotam animal from Africa"s;
	const vector<int> document_rating_2 = {6, 4, 6};
	const int document_id_3 = 2;
	const string document_info_3 = "leopard animal from Africa"s;
	const vector<int> document_rating_3 = {-1, -7, 2, 2};
    {
        SearchServer server;
        server.AddDocument(document_id_1, document_info_1, DocumentStatus::ACTUAL, document_rating_1);
        server.AddDocument(document_id_2, document_info_2, DocumentStatus::ACTUAL, document_rating_2);
        server.AddDocument(document_id_3, document_info_3, DocumentStatus::ACTUAL, document_rating_3);
        const auto found_docs = server.FindTopDocuments("lion"s);
        ASSERT_EQUAL(found_docs.size(), 1); 
    }
}
 
void TestCorrectCalculationNumberFoundDocuments () {
	const int document_id_1 = 0;
	const string document_info_1 = "lion animal from Africa"s;
	const vector<int> document_rating_1 = {6, -1};
	const int document_id_2 = 1;
	const string document_info_2 = "hyppopotam animal from Africa"s;
	const vector<int> document_rating_2 = {6, 4, 6};
	const int document_id_3 = 2;
	const string info_3 = "leopard animal from Africa"s;
	const vector<int> document_rating_3 = {-1, -7, 2, 2};
	{
		SearchServer server;
	    server.AddDocument(document_id_1, document_info_1, DocumentStatus::ACTUAL, document_rating_1);
	    server.AddDocument(document_id_2, document_info_2, DocumentStatus::ACTUAL, document_rating_2);
	    server.AddDocument(document_id_3, info_3, DocumentStatus::ACTUAL, document_rating_3);
	    ASSERT_EQUAL(server.GetDocumentCount(), 3);
	}
}
 
void TestCorectCalculationRelevanceFoundDocuments() {
	SearchServer server;
	const int document_id_1 = 0;
	const vector<int> document_rating_1 = {1};
	string document_info_1 = "веcелый классный кот"s;
	server.AddDocument(document_id_1, document_info_1, DocumentStatus::ACTUAL, document_rating_1);
 
	const int document_id_2 = 1;
	const string document_info_2 = "веселый классный пёс"s;
	const vector<int> document_rating_2 = {2};
	server.AddDocument(document_id_2, document_info_2, DocumentStatus::ACTUAL, document_rating_2);
	const auto found_docs = server.FindTopDocuments("кот"s);
	ASSERT_EQUAL((double)found_docs[0].relevance,(log(server.GetDocumentCount() * 1.0 / 1.0) * (1.0 / 3.0)));
}
 
/*
void TestMatching()
{
        SearchServer server;
        server.AddDocument(1, "Go to the jungle"s, DocumentStatus::ACTUAL,{0, 2, 3});
        server.AddDocument(2, "My name is Ivan"s, DocumentStatus::ACTUAL,{130, -45, 666});
        server.AddDocument(3, "Hello my dear friend"s, DocumentStatus::ACTUAL,{-1, 0, 1});
        vector<string> words;
        DocumentStatus _foo;
        std::tie (words, _foo) = server.MatchDocument("go jungle"s, 1);
        ASSERT_EQUAL(words[0],"jungle"s);
        ASSERT_EQUAL(words[1],"jungle"s);
        std::tie (words, _foo) = server.MatchDocument("jungle"s, 2);
        ASSERT(words.empty());
}
*/

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeStopWordsFromAddingDocumentInfo);
    RUN_TEST(TestExcludeMinusWordsFromAddedDocumentContent);
    RUN_TEST(TestCorrectCalculationRelevanceFoundDocuments);
    RUN_TEST(TestFilteringSearchResultsUserSpecifiedPredicate);
    RUN_TEST(TestReverseSortFoundDocumentsRelevance);
    RUN_TEST(TestCorrectCalculationNumberFoundDocuments);
    RUN_TEST(TestAddingDocument);
    RUN_TEST(TestCorectCalculationRelevanceFoundDocuments);
    RUN_TEST(TestDocumentRatingCalculationAverage);
    RUN_TEST(TestSearchDocumentsWithGivenStatus);
    //RUN_TEST(TestMatching);
}
// --------- Окончание модульных тестов поисковой системы -----------
 
int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
} 