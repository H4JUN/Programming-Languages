from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options
from collections import defaultdict
from bs4 import BeautifulSoup
import time
import datetime
import re
import pandas as pd
import multiprocessing as mp

##############################################
###########  Global Variables ################
##############################################

startDate = "2023-02-26"
endDate = "2023-02-27"
locations = ["서울", "경기", "인천", "충청남도", "충북", "경상남도", "경상북도", "전북", "전남", "강원", "울산", "대전", "광주", "부산", "대구", "제주"]

#############################################
################  Functions  ################
#############################################

# 드라버 불러오기: 밑에 options를 안쓰면 chrome이 headless(백그라운드)로 진행이 안됨. 디폴트 값은 백그라운드로 진행.
def GET_DRIVER(addr = "https://kr.hotels.com", headless = False):
    options = Options()
    if headless:
        options.add_argument("--headless")
        options.add_argument("--allow-insecure-localhost")
        options.add_argument("--user-agent=Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/95.0.4638.69 Safari/537.36")
        options.add_argument("--window-size=1920,1020")
    driver = webdriver.Chrome(options=options)
    driver.get(addr)
    return driver

# 검색 함수: 시군구 단위 주소 입력시 사용
def SEARCH(driver, x):
    print(f"Searching {x}...")
    # Search Button:
    try:
        searchInput = WebDriverWait(driver, 5).until(EC.presence_of_element_located((By.XPATH,"/html/body/div[3]/main/div[1]/div[2]/div/form/div[1]/div/div/div/div/input")))
        searchInput.send_keys(x) # 호텔 이름 입력
        searchInput.send_keys(Keys.ENTER) # 엔터키 입력
    except Exception as e:
        print(f"Could not locate search bar:\n\n{e}\n")
    else:
        time.sleep(1)
        searchButton = WebDriverWait(driver,5).until(EC.element_to_be_clickable((By.XPATH, '/html/body/div[3]/main/div[1]/div[2]/div/form/div[2]/div[3]/button[contains(text(), "검색")]'))) # 검색 버튼 누르기
        searchButton.click()
        time.sleep(3)
    return 

# 실행된 driver의 URL을 바꿔 줌으로써, 검색 날짜를 변경해서 다시 웹페이지를 불러옴
def UPDATE_DRIVER_WITH_NEW_DATE(driver, startdate = startDate, enddate = endDate):
    print(f"Updating search date range: {startdate} -> {enddate}")
    curr_url = driver.current_url
    new_url = re.sub("check-in=[0-9]{4}-[0-9]{2}-[0-9]{2}", f"check-in={startdate}", curr_url) # url 바꿔주기
    new_url2 = re.sub("check-out=[0-9]{4}-[0-9]{2}-[0-9]{2}", f"check-out={enddate}", new_url)
    driver.get(new_url2) # 바뀐 주소로 driver 가동시키기
    time.sleep(5)
    return 

# 쿠키 버튼 체크:
def CHECK_COOKIE(driver):
    print("Checking Cookie...")
    try:
        cookie = WebDriverWait(driver, 5).until(EC.presence_of_element_located((By.XPATH, '/html/body/div[1]/div[2]/div[2]/button[1]')))
        if cookie.text == "동의":
            cookie.click()
            print("Cookie Clicked!")
        else:
            raise Exception
    except:
        print("No Cookie Found.")
    finally:
        time.sleep(1)
    return

# 이상한 팝업 제거
def get_rid_of_useless(driver):
    try:
        useless = WebDriverWait(driver, 5).until(EC.presence_of_element_located((By.XPATH, '/html/body/div[4]/div/div/div/div/header/button')))
        useless.click()
        print("Clicked useless stuff")
    except:
        print("No useless stuff.")
    finally:
        time.sleep(1)
    return


# 만약 호텔스 닷컴의 문제로 로딩이 원활하게 되지 않아 refresh 버튼이 뜰 경우, 눌러주기:
def click_refresh(driver):
    try:
        refresh_button = driver.find_element_by_xpath("/html/body/div[3]/main/div[2]/div/div/div[2]/section[2]/article/button")
    except:
        return
    else:
        refresh_button.click()
        time.sleep(2)
        return

# 인간의 스크롤링을 비슷하게 따라한 방법으로, 호텔스닷컴은 스크롤바가 하단에 도달하면 호텔들이 추가로 로딩이 됌. 밑으로 갔다가, 다시 위로 스크롤링하는것을 반복:
def SCROLL_UNTIL(driver, tolerance = 10):
    curr_len = driver.execute_script("return document.body.scrollHeight")
    counter = 0
    while True:
        # Scroll down to -1400 from bottom:
        driver.execute_script("window.scrollTo({left : 0, top : document.documentElement.scrollHeight - 1400, behavior : 'smooth'});")

        # 수면시간 4초
        time.sleep(4)
        
        # refresh 버튼이 뜨면 눌러주기:
        click_refresh(driver)

        # 카운터를 사용하여, 밑으로 내려가도 로딩이 안될 시, 더이상 호텔이 없다고 판단. counter가 tolerance와 같으면 끝에 도달 했다고 가정.
        new_len = driver.execute_script("return document.body.scrollHeight") # 페이지의 길이를 가져오기
        if curr_len == new_len: # 길이가 같을때마다 카운터를 하나씩 올리기
            counter += 1
        # 길이가 다르면 카운터를 초기화하기:
        elif curr_len != new_len:
            counter = 0
        # 카운터가 tolerance와 같을 경우, 더이상 로딩될 것이 없다고 판단
        if counter == tolerance:
            return True
        driver.execute_script("window.scrollBy({left : 0, top : -document.body.scrollHeight/2, behavior : 'smooth'});") # 페이지의 반 높이로 스크롤 업하기
        time.sleep(2)
        # 현재 페이지 길이 업데이트하기
        curr_len = new_len


# 총 호텔 갯수 가져오는 함수. 호텔 검색시 좌측에 우선적으로 뜨는 정보.
def GET_COUNT(driver):
    elem = WebDriverWait(driver, 5).until(EC.presence_of_element_located((By.XPATH, "/html/body/div[3]/main/div[2]/div/div/div[1]/div/div/div/p")))
    elemtext = elem.text
    numbList = re.findall("[0-9]*", elemtext)
    numb = float("".join(numbList))
    return numb

# html을 가져와서 soup로 변환
def GET_SOUP(driver):
    html = driver.execute_script("return document.body.innerHTML;")
    soup = BeautifulSoup(html, 'html.parser')
    return soup

# soup에서 호텔 아이디를 가진 태그만 찾기
def GET_TARGET_SOUP(soup):
    target = soup.select('li[data-hotel-id]')
    return target

# 데이터 프레임으로 변환: 호텔아이디를 가진 태그만 찾아서 이름, 등급, 주소, 가격을 가져오기:
def COLLECT_TO_DF(target_soup):
    tempDict = defaultdict(list)
    for idx in range(len(target_soup)):
        # Check if NoneType:
        # Name:
        name = target_soup[idx].find('h2', class_="_3zH0kn")
        if name is None: # 이름이 없으면 NULL로 입력
            tempDict["Name"].append("NULL")
        else:
            tempDict["Name"].append(name.get_text().strip())
        # Grade:
        grade = target_soup[idx].find('span', class_="_2dOcxA")
        if grade is None:
            tempDict["Grade"].append("NULL")
        else:
            tempDict["Grade"].append(grade.get_text().strip())
        # Address
        addr = target_soup[idx].find('p', class_="_2oHhXM")
        if addr is None:
            tempDict["Address"].append("NULL")
        else:
            tempDict["Address"].append(addr.get_text().strip())
        # Price
        price = target_soup[idx].find('span', class_="_2R4dw5")
        if price is None:
            tempDict["Price"].append("NULL")
        else:
            tempDict["Price"].append(price.get_text().strip())
    tempDf = pd.DataFrame(tempDict)
    return tempDf # 데이터프레임 반환

# 엑셀 파일로 저장시:
def EXPORT(df, location):
    timestamp = time.strftime("%y-%m-%d-%H:%M:%S", time.localtime())
    df.to_excel(f"hotel{location}:{startDate}->{endDate}{timestamp}.xlsx")
    return


######################################################
######################
################  CRAWL함수로 검색 진행. main()대신이라고 생각하시면 됩니다.
#####################
######################################################
def crawl(location):
    while True:
        try:
            starttime = time.time()
            driver = GET_DRIVER()
            driver.maximize_window()
            CHECK_COOKIE(driver)
            get_rid_of_useless(driver)
            SEARCH(driver, location)
            # 날짜 업데이트
            UPDATE_DRIVER_WITH_NEW_DATE(driver)

            count = GET_COUNT(driver)
            scrolled = SCROLL_UNTIL(driver)
            # 끝까지 스크롤링해서 더 이상 호텔이 안 추가 될 경우:
            if scrolled:
                time.sleep(2)
                soup = GET_SOUP(driver)
                target = GET_TARGET_SOUP(soup)
                df = COLLECT_TO_DF(target)
                print(df)
                print(f"{len(df)} matched. Supposed to find {count} number of items?")
                print(f"Matched Rate: {round(len(df)/count * 100, 1)}%")
                EXPORT(df, location)
                print(f"\n\n----{time.time() - starttime} seconds----\n\n")
            time.sleep(3)
            print(f"\n\n{location} Done!\n\n")
            driver.close()
        except:
            # 오류 발생시 다시 검색:
            print(f"Retrying {location}...")
            crawl(location)
            break
        else:
            # 모든게 잘 진행 될 시:
            break
    return

if __name__ == "__main__":
    # 멀티프로세싱: 병렬 처리
    with mp.Pool() as pool:
       starttime1 = time.time()
       pool.map(crawl, locations)
       print(f"\n\nTotal Time Taken:\n----{time.time() - starttime1} seconds----\n\n")
       print("\n\nAll Done!\n\n")
    Testing:
