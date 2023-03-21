#include "process_queries.h"
#include <algorithm>
#include <execution>

using namespace std;

// ������� ������ ���������� ��� ������� ������� � �������������� �����������������. ���������� ������ ����������� � ������������ � ������� result
vector<vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const vector<string>& queries) {
    vector<vector<Document>> result(queries.size());
    transform(execution::par_unseq, queries.begin(), queries.end(), result.begin(),
        [&search_server](const auto& query) { return search_server.FindTopDocuments(query); });
    return result;
}

// ��������� ������� ������� ���� ����������. ���������� ������ ����������� � ������������ � ������� ��������(ProcessQueriss) documents
std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> documents_lists = ProcessQueries(search_server, queries);
    std::vector<Document> documents;
    for (auto& docs : documents_lists) {
        documents.insert(documents.end(), docs.begin(), docs.end());
    }
    return documents;
}