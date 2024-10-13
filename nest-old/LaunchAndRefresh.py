from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.common.by import By
import time

panning_constant = 100
total_pan = [0.0, 0.0]
max_zoom = 18
min_zoom = 10
initial_zoom = 15
current_zoom = 15

def ZoomIn():
    global current_zoom
    if current_zoom < max_zoom:
        current_zoom += 1
        driver.find_element(by=By.CSS_SELECTOR, value=".leaflet-control-zoom-in").click()

def ZoomOut():
    global current_zoom
    if current_zoom > min_zoom:
        current_zoom -= 1
        driver.find_element(by=By.CSS_SELECTOR, value=".leaflet-control-zoom-out").click()

def PanUp():
    webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(0, panning_constant).release().perform()
    total_pan[1] += panning_constant* (2**(initial_zoom-current_zoom))

def PanDown():
    webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(0, -panning_constant).release().perform()
    total_pan[1] -= panning_constant* (2**(initial_zoom-current_zoom))

def PanLeft():
    webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(-panning_constant, 0).release().perform()
    total_pan[0] -= panning_constant* (2**(initial_zoom-current_zoom))

def PanRight():
    webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(panning_constant, 0).release().perform()
    total_pan[0] += panning_constant* (2**(initial_zoom-current_zoom))

def Relaunch():
    global current_zoom, total_pan
    driver.get("file:\\C:\\Users\\Maanav\\Desktop\\School\\CPE450\\CapstoneBuoyProject\\map.html")
    webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(total_pan[0], total_pan[1]).release().perform()
    if current_zoom < initial_zoom:
        for i in range(initial_zoom - current_zoom):
            ZoomOut()
    elif current_zoom > initial_zoom:
        for i in range(current_zoom - initial_zoom):
            ZoomIn()

    total_pan = [0.0, 0.0]
    current_zoom = initial_zoom

# Start Firefox browser
driver = webdriver.Firefox()
    # Open the Folium map URL
driver.get("file:\\C:\\Users\\Maanav\\Desktop\\School\\CPE450\\CapstoneBuoyProject\\map.html")

time.sleep(5)

    # Wait for the map to load

    # Zoom in on the map (for demonstration purposes)
#driver.find_element(by=By.CSS_SELECTOR, value=".leaflet-control-zoom-in").click()
# driver.find_element(by=By.CSS_SELECTOR, value=".leaflet-control-zoom-out").click()
# marker_elements = driver.find_elements(by=By.CSS_SELECTOR, value='.awesome-marker-icon-darkgreen')
# print(marker_elements)
# # Click on each marker
# for marker in marker_elements:
#     marker.click()

    # # Pan the map (for demonstration purposes)
# map_element = driver.find_element(by=By.CLASS_NAME, value="leaflet-container")
# webdriver.ActionChains(driver).click_and_hold(map_element).move_by_offset(100, 100).release().perform()
PanUp()
ZoomIn()
PanRight()
print(total_pan)


time.sleep(5)
Relaunch()
print(total_pan)
time.sleep(5)