import psycopg2, sys, geopandas as gpd

if len(sys.argv) != 2:
    print("Usage: python initializeDeckbox.py [ID]")
    exit()

connection = psycopg2.connect(
        user="map",
        password="superdupersecret2",
        host="localhost",
        port="5432",
        database="postgres"
    )

connection.autocommit = True
cursor = connection.cursor()

drop_table_query = "DROP TABLE IF EXISTS deckbox"
cursor.execute(drop_table_query)

drop_table_query = "DROP TABLE IF EXISTS buoys"
cursor.execute(drop_table_query)

drop_table_query = "DROP TABLE IF EXISTS last_update"
cursor.execute(drop_table_query)

create_table_query = '''CREATE TABLE deckbox(id text PRIMARY KEY, recorded_at int, longitude real, latitude real)'''
cursor.execute(create_table_query)

create_table_query = '''CREATE TABLE buoys(deckbox_id_buoy_id text PRIMARY KEY, recorded_at int, longitude real, latitude real, event_type text, dirty bool)'''
cursor.execute(create_table_query)

create_table_query = '''CREATE TABLE last_update(tstamp text PRIMARY KEY)'''
cursor.execute(create_table_query)

insert_buoy_query = '''INSERT INTO deckbox (id, recorded_at, longitude, latitude)
    VALUES (%s, %s, %s, %s)
    '''

cursor.execute(insert_buoy_query, (sys.argv[1], 0, 1000, 1000))
