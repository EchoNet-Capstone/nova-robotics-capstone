import psycopg2, sys, time

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


connection = psycopg2.connect(
        user="map",
        password="superdupersecret2",
        host="localhost",
        port="5432",
        database="postgres"
    )

connection.autocommit = True

main_cursor = connection.cursor()

drop_table_query = "DROP TABLE IF EXISTS buoys"
main_cursor.execute(drop_table_query)

create_table_query = '''CREATE TABLE buoys(deckbox_id_buoy_id text PRIMARY KEY, recorded_at int, longitude real, latitude real, event_type text, dirty bool)'''
main_cursor.execute(create_table_query)


main_cursor.execute(update_deckbox, (int(time.time()), -120.66482, 35.30220))


records_to_insert = [
        ('1-1', 1644652800, -120.672, 35.292, 'gear_deployed', True),
        ('2-2', 1644652800, -120.668, 35.294, 'gear_deployed', True),
        ('5678-3', 1644652800, -120.676, 35.288, 'gear_deployed', True),
        ('4-4', 1644652800, -120.672, 35.286, 'gear_deployed', True),
        ('5678-5', 1644652800, -120.680, 35.300, 'gear_deployed', True),
        # ('1-1', 1644652800, -122.410, 37.775, 'gear_deployed', True),
        # ('6-2', 1644652800, -122.410, 37.770, 'gear_deployed', True),
        # ('9-3', 1644652800, -122.410, 37.765, 'gear_deployed', True),
        # ('11-4', 1644652800, -122.410, 37.760, 'gear_deployed', True),
        # ('6-5', 1644652800, -122.410, 37.755, 'gear_deployed', True),
    ]

main_cursor.executemany(insert_buoy, records_to_insert)
