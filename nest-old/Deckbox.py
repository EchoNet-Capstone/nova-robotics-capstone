#!/usr/bin/python3
import folium
from geopy.distance import distance
from selenium import webdriver
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.common.by import By
from selenium.webdriver.firefox.options import Options
from selenium.webdriver.firefox.service import Service
import tkinter as tk
#import tk
import geopandas as gpd
import serial
import threading
import time

import sys
import os
import psycopg2

import odroid_wiringpi as wpi

from odroid_wiringpi import digitalRead

ZOOM_OUT_PIN = 17    # physical pin 13
ZOOM_IN_PIN  = 106   # physical pin 15
#SELECT_PIN   = 145   # physical pin 29
PAN_UP     = 119 #physical pin 18
PAN_DOWN   = 118 #physical pin 16
PAN_RIGHT  = 120 #physical pin 12
PAN_LEFT   = 121 #physical pin 22 
PAIR = 145 #used to initiate pairing


#.cx[bbox[0]:bbox[2], bbox[1]:bbox[3]]
inner_radius = 5
lat_bath_clip = 1.5
long_bath_clip = 3
lat_feature_clip = 5
long_feature_clip = 7.5

map_imports_dir = "./Map_Imports/"
file_loc = "home\\odroid\\CapstoneBuoyProject\\map.html"
connection = None
my_deckbox_id = None
driver = None
arduino = None
map_delay = 60
db_delay = 10

panning_constant = 100
total_pan = [0.0, 0.0]
max_zoom = 18
min_zoom = 10
initial_zoom = 15
current_zoom = 15

mutex = threading.Lock()

colors = ["#570000",
          "#078223",
          "#003585",
          "#6E6E6E"
          ]

select_buoys = "SELECT * FROM buoys"
select_deckbox = "SELECT * FROM deckbox"

insert_buoy = """
        INSERT INTO buoys (deckbox_id_buoy_id, recorded_at, longitude, latitude, event_type, dirty)
        VALUES (%s, %s, %s, %s, %s, %s)
    """

update_deckbox = """
        UPDATE deckbox
        SET recorded_at = %s, longitude = %s, latitude = %s
    """

pull_single = "SELECT * FROM buoys WHERE deckbox_id_buoy_id = %s"

delete_single = "DELETE FROM buoys WHERE deckbox_id_buoy_id = %s"

bboxs = {
"Alaska_WestCanada": (-166.921876,49,-124.734376,61.528962),
"WestUSA": (-130.501797,30.286396,-116.015625,49),
"GulfofMexico": (-97.971678,21.375298,-75.280273,30.5),
"SoutheastUSA": (-81.624023,30.5,-64.001954,42.0),
"NewEngland_EastCanada": (-71.740723,42.0,-46.494141,56.161294)
}

def insert_num(num):
    entry_text.set(entry_text.get() + str(num))

def clear():
    entry_text.set("")

def get_input():
    global entry_text

    root = tk.Tk()
    root.withdraw()
    entry_text = tk.StringVar()
    num_pad = tk.Toplevel(root)

    entry_frame = tk.Frame(num_pad)
    entry_frame.pack()

    entry_text.set("")
    entry = tk.Entry(entry_frame, textvariable=entry_text, state="readonly", font=('Arial', 36))
    entry.pack()

    button_frame = tk.Frame(num_pad)
    button_frame.pack(fill="both", expand=True)

    for i in range(1, 10):
        tk.Button(button_frame, text=str(i), width=5, height=5, command=lambda i=i: insert_num(i), font=('Arial', 16)).grid(row=(i-1)//3, column=(i-1)%3, sticky="NSEW")

    tk.Button(button_frame, text="0", width=5, height=5, command=lambda: insert_num(0), font=('Arial', 16)).grid(row=3, column=1, sticky="NSEW")
    tk.Button(button_frame, text="Clear", width=5, height=5,command=clear, font=('Arial', 16)).grid(row=3, column=0, sticky="NSEW")
    tk.Button(button_frame, text="Enter", width=5, height=5, command=num_pad.destroy, font=('Arial', 16)).grid(row=3, column=2, sticky="NSEW")

    for i in range(3):
        button_frame.grid_columnconfigure(i, weight=1)

    num_pad.grab_set()
    num_pad.wait_window()

    return entry_text.get()


def setup():
    wpi.wiringPiSetupGpio()

    wpi.pinMode(ZOOM_OUT_PIN, wpi.INPUT) 
    wpi.pinMode(ZOOM_IN_PIN, wpi.INPUT)  
    #wpi.pinMode(SELECT_PIN, wpi.INPUT)
    wpi.pinMode(PAN_UP, wpi.INPUT)
    wpi.pinMode(PAN_DOWN, wpi.INPUT)
    wpi.pinMode(PAN_RIGHT, wpi.INPUT)
    wpi.pinMode(PAN_LEFT, wpi.INPUT)
    wpi.pinMode(PAIR, wpi.INPUT)

    wpi.pullUpDnControl(ZOOM_OUT_PIN, wpi.PUD_DOWN)
    wpi.pullUpDnControl(ZOOM_IN_PIN, wpi.PUD_DOWN)
    #wpi.pullUpDnControl(SELECT_PIN, wpi.PUD_DOWN)
    wpi.pullUpDnControl(PAN_UP, wpi.PUD_DOWN)
    wpi.pullUpDnControl(PAN_DOWN, wpi.PUD_DOWN)
    wpi.pullUpDnControl(PAN_RIGHT, wpi.PUD_DOWN)
    wpi.pullUpDnControl(PAN_LEFT, wpi.PUD_DOWN)
    wpi.pullUpDnControl(PAIR, wpi.PUD_DOWN)



def readControls():
    while True:
        if(digitalRead(ZOOM_IN_PIN) == wpi.HIGH):
            print("ZOOM IN")
            ZoomIn() 
            time.sleep(1)
            
        if(digitalRead(ZOOM_OUT_PIN) == wpi.HIGH):
            print("ZOOM OUT")
            ZoomOut()
            time.sleep(1)
        
     #   if(digitalRead(SELECT_PIN) == wpi.HIGH):
      #      print("SELECT")
       #     time.sleep(1)

        if(digitalRead(PAN_UP) == wpi.HIGH):
            print("UP")
            PanUp()
            time.sleep(1)

        if(digitalRead(PAN_DOWN) == wpi.HIGH):
            print("DOWN")
            PanDown()
            time.sleep(1)
            
        if(digitalRead(PAN_RIGHT) == wpi.HIGH):
            print("RIGHT")
            PanRight()
            time.sleep(1)
        
        if(digitalRead(PAN_LEFT) == wpi.HIGH):
            print("LEFT")
            PanLeft()
            time.sleep(1)

        if(digitalRead(PAIR) == wpi.HIGH):
            print("PAIR")
            Pair()
            time.sleep(1)

        

def ZoomIn():
    global current_zoom
    mutex.acquire()
    if current_zoom < max_zoom:
        current_zoom += 1
        #driver.find_element(by=By.CSS_SELECTOR, value=".leaflet-control-zoom-in").click().release()
        webdriver.ActionChains(driver).click(driver.find_element(by=By.CLASS_NAME, value="leaflet-control-zoom-in")).perform()
    mutex.release()

def ZoomOut():
    global current_zoom
    mutex.acquire()
    if current_zoom > min_zoom:
        current_zoom -= 1
        #driver.find_element(by=By.CSS_SELECTOR, value=".leaflet-control-zoom-out").click()
        webdriver.ActionChains(driver).click(driver.find_element(by=By.CLASS_NAME, value="leaflet-control-zoom-out")).perform()
    mutex.release()

def PanUp():
    mutex.acquire()
    webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(0, panning_constant).release().perform()
    total_pan[1] += panning_constant* (2**(initial_zoom-current_zoom))
    mutex.release()

def PanDown():
    mutex.acquire()
    webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(0, -panning_constant).release().perform()
    total_pan[1] -= panning_constant* (2**(initial_zoom-current_zoom))
    mutex.release()

def PanLeft():
    mutex.acquire()
    webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(panning_constant, 0).release().perform()
    total_pan[0] += panning_constant* (2**(initial_zoom-current_zoom))
    mutex.release()

def PanRight():
    mutex.acquire()
    webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(-panning_constant, 0).release().perform()
    total_pan[0] -= panning_constant* (2**(initial_zoom-current_zoom))
    mutex.release()

def Pair():
    pairing_code = get_input()
    print(pairing_code)
    arduino.write(f'PAIR:{pairing_code}'.encode())

def Relaunch():
    global current_zoom, total_pan
    mutex.acquire()
    driver.get("file:\\"  + file_loc)
    print(total_pan)
    if total_pan[0] != 0 or total_pan[1] != 0:
        for i in range(initial_zoom - min_zoom):
            webdriver.ActionChains(driver).click(driver.find_element(by=By.CLASS_NAME, value="leaflet-control-zoom-out")).perform()
        webdriver.ActionChains(driver).click_and_hold(driver.find_element(by=By.CLASS_NAME, value="leaflet-container")).move_by_offset(total_pan[0]* (2**(min_zoom-initial_zoom)), total_pan[1]* (2**(min_zoom-initial_zoom))).release().perform()
        for i in range(current_zoom - min_zoom):
            webdriver.ActionChains(driver).click(driver.find_element(by=By.CLASS_NAME, value="leaflet-control-zoom-in")).perform()
    else:
        if current_zoom < initial_zoom:
            for i in range(initial_zoom - current_zoom):
                #driver.find_element(by=By.CSS_SELECTOR, value=".leaflet-control-zoom-out").click()
                webdriver.ActionChains(driver).click(driver.find_element(by=By.CLASS_NAME, value="leaflet-control-zoom-out")).perform()
        elif current_zoom > initial_zoom:
            for i in range(current_zoom - initial_zoom):
            # driver.find_element(by=By.CSS_SELECTOR, value=".leaflet-control-zoom-in").click()
                webdriver.ActionChains(driver).click(driver.find_element(by=By.CLASS_NAME, value="leaflet-control-zoom-in")).perform()

    total_pan = [0.0, 0.0]
    current_zoom = initial_zoom
    mutex.release()
    
def findCoast(center_coords):
    for coast, bbox in bboxs.items():
        if bbox[0] <= center_coords[1] <= bbox[2] and bbox[1] <= center_coords[0] <= bbox[3]:
            print("Deckbox is located on", coast, "coast")
            return coast
    
    raise ValueError("Deckbox position is not within mapped regions")

def generateMap():
    global driver
    coast = None

    map_cursor = connection.cursor()
    while True:
        map_cursor.execute(select_deckbox)
        grab_deckbox = map_cursor.fetchall()[0]

        if grab_deckbox[3] != 1000:
            center_coords = (grab_deckbox[3], grab_deckbox[2]) # lat, long
            bath_bbox = (center_coords[1]-long_bath_clip,center_coords[0]-lat_bath_clip,center_coords[1]+long_bath_clip,center_coords[0]+lat_bath_clip)
            feature_bbox = (center_coords[1]-long_feature_clip,center_coords[0]-lat_feature_clip,center_coords[1]+long_feature_clip,center_coords[0]+lat_feature_clip)
            if coast is None or not(bboxs[coast][0] <= center_coords[1] <= bboxs[coast][2] and bboxs[coast][1] <= center_coords[0] <= bboxs[coast][3]):
                coast = findCoast(center_coords)
            
                countries_df = gpd.read_file(map_imports_dir + "Countries_" + coast + ".shp", engine='pyogrio')
                lines_df = gpd.read_file(map_imports_dir + "Bathymetry_" + coast + ".shp", engine='pyogrio')
                oceans_df = gpd.read_file(map_imports_dir + "Ocean_" + coast + ".shp", engine='pyogrio')

            map_cursor.execute(select_buoys)
            points = map_cursor.fetchall()

            # for row in points:

            map_obj = folium.Map(location=center_coords, tiles=None, minZoom=min_zoom, zoom_start=initial_zoom, maxZoom=max_zoom, control_scale=True, max_bounds=True, zoom_control=True, min_lat=max(-90, center_coords[0]-lat_feature_clip), min_lon=max(-180, center_coords[1]-long_feature_clip), max_lat=min(90, center_coords[0]+lat_feature_clip), max_lon=min(180, center_coords[1]+long_feature_clip))
            folium.GeoJson(name="Countries", data=gpd.clip(countries_df,feature_bbox)[["ISO_A3_EH", "geometry"]].to_json(), style_function=lambda x: {"color": "#34A56F", "weight": 1.0}).add_to(map_obj)
            oceans_json = folium.GeoJson(name="Oceans", data=gpd.clip(oceans_df,feature_bbox).to_json(), style_function=lambda x: {"weight": 0.5}).add_to(map_obj)
            lines_json = folium.GeoJson(name="Bathymetry", data=gpd.clip(lines_df,bath_bbox).to_json(), style_function=lambda x: {"weight": 0.5}).add_to(map_obj)

            folium.Marker(location=center_coords, popup="Your Location\nTimestamp: {}".format(time.strftime("%m-%d-%Y %H:%M:%S", time.localtime(grab_deckbox[1]))), icon=folium.Icon(prefix="fa", color="darkblue", icon="arrow-down")).add_to(map_obj)
            #my_marker_cluster = MarkerCluster().add_to(map_obj)

            for point in points:
                point_coords = (point[3], point[2])
                point_distance = distance(center_coords, point_coords).miles
                deckbox_id = point[0].split("-")[0]
                buoy_id = point[0].split("-")[1]
                # Check if the point is within the specified radius
                # IF GEAR RETRIEVED, DONT DISPLAY
                if point[4] == "gear_deployed":
                    if deckbox_id == my_deckbox_id:
                        folium.Marker(location=point_coords, popup="Deckbox ID: {}\nBuoy ID: {}\nTimestamp: {}".format(deckbox_id, buoy_id, time.strftime("%m-%d-%Y %H:%M:%S", time.localtime(point[1]))), icon=folium.Icon(prefix="fa", icon="life-ring", color="darkgreen")).add_to(map_obj)
                    elif point_distance < inner_radius:
                        folium.Marker(location=point_coords, popup="Deckbox ID: {}\nBuoy ID: {}\nTimestamp: {}".format(deckbox_id, buoy_id, time.strftime("%m-%d-%Y %H:%M:%S", time.localtime(point[1]))), icon=folium.Icon(prefix="fa", icon="life-ring", color="black")).add_to(map_obj)
                elif point[4] == "gear_lost":
                    folium.Marker(location=point_coords, popup="Deckbox ID: {}\nBuoy ID: {}\nTimestamp: {}".format(deckbox_id, buoy_id, time.strftime("%m-%d-%Y %H:%M:%S", time.localtime(point[1]))), icon=folium.Icon(prefix="fa", icon="life-ring", color="darkred")).add_to(map_obj)
            
            layers = folium.LayerControl().add_to(map_obj)
            map_obj.save('map.html')
           # Relaunch()
        print("Map Generated, Sleeping for", map_delay, "seconds")
        #time.sleep(map_delay - ((time.monotonic() - starttime) % map_delay))
        time.sleep(map_delay)

    map_cursor.close()

def clear_temp_folder():
    # Iterate over all files in the folder
    for file_name in os.listdir("../tmp"):
        file_path = os.path.join("../tmp", file_name)
        # Check if the current item is a file
        if os.path.isfile(file_path):
            # If it's a file, remove it
            os.remove(file_path)

def main():
    global connection, my_deckbox_id, driver, arduino
    if len(sys.argv) != 2:
        print("Usage: python cubeCelltoEarthranger.py [DEVPORT]")
        return
    
    clear_temp_folder()
    
    connection = psycopg2.connect(
        user="map",
        password="superdupersecret2",
        host="localhost",
        port="5432",
        database="postgres"
    )

    arduino = serial.Serial(sys.argv[1], timeout=1, baudrate=115200)

    options = Options()
    options.binary_location = '/snap/bin/firefox'
    service = Service('/usr/bin/geckodriver')
    print("Launching Firefox")
    driver = webdriver.Firefox(options=options, service=service)
    driver.maximize_window()
    driver.get("file:\\" + file_loc)

    connection.autocommit = True

    main_cursor = connection.cursor()
    
    setup()
    
    main_cursor.execute(select_deckbox)
    my_deckbox_id = main_cursor.fetchall()[0][0]

    print("Launching Threads")
    threading.Thread(target=generateMap, daemon=True).start()
    threading.Thread(target=updateDBLoop, daemon=True).start()
    
    # while True:
    #    time.sleep(60)

    readControls()

def updateDBLoop():
    serial_cursor = connection.cursor()
    while True:
        line = arduino.readline().decode("utf-8")
        print(line)
        curr_time = time.time()
        while len(line) > 0:
            if line.startswith("DECKBOX"):
                latitude = float(blocks[3][:-1])
                longitude = float(blocks[6][:-1])
                timestamp = int(curr_time - (int(blocks[9][:-1]) / 1000))

                serial_cursor.execute(update_deckbox, (timestamp, longitude, latitude))

            elif line.startswith("Type:"):
                blocks = line.split()
                timestamp = int(curr_time - (int(blocks[11][:-1]) * 60))
                id = blocks[3][:-1] + "-" + blocks[5][:-1]
                serial_cursor.execute(pull_single, (id,))
                buoy_ret = serial_cursor.fetchall()
                if len(buoy_ret) == 0 or timestamp > buoy_ret[0][1]:

                    latitude = float(blocks[7][:-1])
                    longitude = float(blocks[9][:-1])
                    if blocks[1][:-1] == "7":
                        state = 'gear_lost'
                    else:
                        state = 'gear_deployed'
                    print("Deckbox-Buoy ID:", id)
                    print("Timestamp:", timestamp)
                    print("Latitude:", latitude)
                    print("Longitude:",  longitude)

                    serial_cursor.execute(delete_single, (id,))
                    serial_cursor.execute(insert_buoy, (id, timestamp, longitude, latitude, state, True))

            line = arduino.readline().decode("utf-8")
            print(line)

        print("Parsed all serial, sleeping for", db_delay, "seconds")
        time.sleep(db_delay)

    serial_cursor.close()
    connection.close()

if __name__ == "__main__":
    main()
