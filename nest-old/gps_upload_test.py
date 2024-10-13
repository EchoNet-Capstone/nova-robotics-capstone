import requests

def update_buoy_pos(buoy_id: str, recorded_at:str, longitude:float, latitude:float, gear_removed:bool = False, debug:bool = False):
    event_data = [{
        "device_id": buoy_id,
        "recorded_at": recorded_at,
        "location":{
            "x":longitude,
            "y":latitude,
        },
        "event_type": "gear_retrieved" if gear_removed else "gear_deployed",
    }]
    headers = {"apikey":"duNL2uc2O4DNRBCQNu9swRO5OofEaxFa","accept":"application/json","Content-Type":"application/json"}
    req = requests.post('https://cdip-api-prod01.pamdas.org/events/', json=event_data, headers=headers)
    if debug: print(req.text)

update_buoy_pos("buoy02", "2023-11-25T12:23:52", -14.085786, 41.584472, False, True)