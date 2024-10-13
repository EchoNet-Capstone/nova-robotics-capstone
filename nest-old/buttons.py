import odroid_wiringpi as wpi
import time

ZOOM_OUT_PIN = 17    # physical pin 13
ZOOM_IN_PIN  = 106   # physical pin 15
SELECT_PIN   = 145   # physical pin 29
PAN_UP     = 119 #physical pin 18
PAN_DOWN   = 118 #physical pin 16
PAN_RIGHT  = 120 #physical pin 12
PAN_LEFT   = 121 #physical pin 22 

def setup():
    wpi.wiringPiSetupGpio()

    wpi.pinMode(ZOOM_OUT_PIN, wpi.INPUT) 
    wpi.pinMode(ZOOM_IN_PIN, wpi.INPUT)  
    wpi.pinMode(SELECT_PIN, wpi.INPUT)
    wpi.pinMode(PAN_UP, wpi.INPUT)
    wpi.pinMode(PAN_DOWN, wpi.INPUT)
    wpi.pinMode(PAN_RIGHT, wpi.INPUT)
    wpi.pinMode(PAN_LEFT, wpi.INPUT)

    wpi.pullUpDnControl(ZOOM_OUT_PIN, wpi.PUD_DOWN)
    wpi.pullUpDnControl(ZOOM_IN_PIN, wpi.PUD_DOWN)
    wpi.pullUpDnControl(SELECT_PIN, wpi.PUD_DOWN)
    wpi.pullUpDnControl(PAN_UP, wpi.PUD_DOWN)
    wpi.pullUpDnControl(PAN_DOWN, wpi.PUD_DOWN)
    wpi.pullUpDnControl(PAN_RIGHT, wpi.PUD_DOWN)
    wpi.pullUpDnControl(PAN_LEFT, wpi.PUD_DOWN)

def readControls()
    while True:
        if(digitalRead(ZOOM_IN_PIN) == wpi.HIGH):
            print("ZOOM IN") 
            time.sleep(1)
            
        if(digitalRead(ZOOM_OUT_PIN) == wpi.HIGH):
            print("ZOOM OUT")
            time.sleep(1)
        
        if(digitalRead(SELECT_PIN) == wpi.HIGH):
            print("SELECT")
            time.sleep(1)

        if(digitalRead(PAN_UP) == wpi.HIGH):
            print("UP")
            time.sleep(1)

        if(digitalRead(PAN_DOWN) == wpi.HIGH):
            print("DOWN") 
            time.sleep(1)
            
        if(digitalRead(PAN_RIGHT) == wpi.HIGH):
            print("RIGHT")
            time.sleep(1)
        
        if(digitalRead(PAN_LEFT) == wpi.HIGH):
            print("LEFT")
            time.sleep(1)

def main():
    setup()
    readControls()

if __name__ == '__main__':
    main()
