#include "request_queue.h" 
#include <numeric> 
using namespace std;

// Ќовый запрос на поиск в очереди запросов класса Request_queue
vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus document_status) {
    return AddFindRequest(raw_query,
        [document_status]([[maybe_unused]] int document_id, DocumentStatus status,
            [[maybe_unused]] int rating) { return status == document_status; });
}

//  оличество запросов на поиск документов, не найдено ни одного документа
int RequestQueue::GetNoResultRequests() const {
    return empty_responses_count_;
}

// ‘ункци€ обновлени€ информации о запросах на поиск документов на основе ответа сервера
void RequestQueue::UpdateRequestsInformation(const vector<Document>& response) {
    if (requests_.size() == kMinutesInDay) {
        if (requests_.front().IsEmpty())
            --empty_responses_count_;
        requests_.pop_front();
    }
    if (response.empty())
        ++empty_responses_count_;
}