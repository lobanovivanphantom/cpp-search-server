#pragma once 
#include <vector> 
#include <string> 
#include <set>
#include <string_view>



std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWords(const std::string_view& text);



template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings;
    for (const std::string_view& str : strings) {
        if (!str.empty()) {
            std::string temp(str.begin(), str.end());
            non_empty_strings.insert(temp);
        }
    }
    return non_empty_strings;
}
