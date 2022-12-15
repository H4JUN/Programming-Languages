from bs4 import BeautifulSoup
import requests as r
import pickle
import re
import time
import multiprocessing as mp

pair_list = mp.Manager().list() # shared memory
lock = mp.Lock()

def main():
    with open("url_jusoga", "rb") as f:
        regions_urls = pickle.load(f)
    
    def generate_sublists(x, n):
        sublen = len(x) // n
        sublists = []
        for i in range(0, len(x), sublen):
            sublists.append(x[i:i+sublen])
        return sublists
    
    splitted_list = generate_sublists(regions_urls, 16)
    global get_addresses # function defined inside the scope of main(). multiprocessing uses pickles to pass data through processes. inner function are locally scoped and thus cannot be serialized. Make it global
    def get_addresses(url_list):
        global lock
        global pair_list
        for url in url_list:
            response = r.get(url)
            if response.status_code == 200:
                html = response.text
                soup = BeautifulSoup(html, "html.parser")
                td_pair_list = []
                for tr_tags in soup.find_all("tr"):
                    td_pair = []
                    for row in tr_tags.find_all("td"):
                        td_pair.append(row.text)
                    td_pair_list.append(td_pair)
                td_pair_list = [item for item in td_pair_list if len(item) > 0]
                with lock:
                    pair_list.extend(td_pair_list)
                time.sleep(1)
            else:
                print(f"Something went wrong with {url}")
                continue
    
    with mp.Pool(16) as p:
        p.map(get_addresses, splitted_list)
    
    with open("pairs.pkl", "wb") as f:
        pickle.dump(pair_list._getvalue(), f)
    
    return

if __name__ == "__main__":
    main()
