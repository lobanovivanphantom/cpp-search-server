#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<int> duplicates_documents;
    set<set<string>> documents;
    for (const int document_id : search_server) {
        const auto& document_freq = search_server.GetWordFrequencies(document_id);
        set<string> words_in_doc;

        for (const auto& [word, freq] : document_freq) {
            words_in_doc.insert(word);
            
        }
       
        if (documents.count(words_in_doc)) {
            duplicates_documents.insert(document_id);
        }
        else {
            documents.insert(words_in_doc);
        }
    }

   


    for (auto id : duplicates_documents) {
        cout << "Found duplicate document id "s << id << endl;
       search_server.RemoveDocument(id);
    }
    
}