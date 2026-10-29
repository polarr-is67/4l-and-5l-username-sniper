#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <curl/curl.h>

using namespace std;

bool stop_now = false;
mutex print_lock;
string chars = "abcdefghijklmnopqrstuvwxyz0123456789_";

unsigned long long rng_state;

unsigned long long xorshift64() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

void manual_shuffle(vector<long long>& arr) {
    for (long long i = arr.size() - 1; i > 0; i--) {
        long long j = xorshift64() % (i + 1);
        long long temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

size_t write_data(void* ptr, size_t size, size_t nmemb, string* s) {
    s->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

void do_work(int len, vector<long long> my_list) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/120.0.0.0");

    string url = "https://discord.com/api/v9/unique-username/username-attempt-unauthed";
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    int counter = 0;

    for (long long id : my_list) {
        if (stop_now) break;

        string name = "";
        long long tmp = id;
        for (int i = 0; i < len; i++) {
            name = chars[tmp % chars.length()] + name;
            tmp /= chars.length();
        }

        counter++;
        if (counter % 50 == 0) {
            lock_guard<mutex> lock(print_lock);
            cout << "\033[36mchecking " << name << "\033[0m\r" << flush;
        }

        string resp;
        string body = "{\"username\":\"" + name + "\"}";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

        CURLcode res = curl_easy_perform(curl);
        long code = 0;
        
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            
            if (code == 200 && resp.find("\"taken\":false") != string::npos) {
                lock_guard<mutex> lock(print_lock);
                cout << "\n\033[32mFOUND FREE USERNAME: " << name << "\033[0m\n";
                stop_now = true;
                break;
            }
            else if (code == 429) {
                this_thread::sleep_for(chrono::seconds(5));
            }
        }
        
        this_thread::sleep_for(chrono::milliseconds(20));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    int len;

    cout << "4 or 5 letters? ";
    cin >> len;

    if (len != 4 && len != 5) {
        cout << "i said 4 or 5 bro\n";
        return 0;
    }

    rng_state = chrono::high_resolution_clock::now().time_since_epoch().count();
    if (rng_state == 0) rng_state = 88172645463325252ULL; 

    int cpus = 4; 

    long long total = 1;
    for (int i = 0; i < len; i++) total *= chars.length();

    cout << "making a list of " << total << " names...\n";
    vector<long long> nums(total);
    for (long long i = 0; i < total; i++) nums[i] = i;

    manual_shuffle(nums);

    long long part = total / cpus;
    cout << "starting " << cpus << " threads...\n";

    vector<thread> workers;
    for (int t = 0; t < cpus; t++) {
        auto start = nums.begin() + (t * part);
        auto end = (t == cpus - 1) ? nums.end() : (start + part);
        vector<long long> chunk(start, end);
        workers.push_back(thread(do_work, len, chunk));
    }

    for (auto& w : workers) w.join();

    cout << "\ndone.\n";
    curl_global_cleanup();
    return 0;
}
