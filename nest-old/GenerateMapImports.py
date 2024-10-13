import folium
import geopandas as gpd

bboxs = {
"Alaska_WestCanada": (-166.921876,49,-124.734376,61.528962),
"WestUSA": (-130.501797,30.286396,-116.015625,49),
"GulfofMexico": (-97.971678,21.375298,-75.280273,30.5),
"SoutheastUSA": (-81.624023,30.5,-64.001954,42.0),
"NewEngland_EastCanada": (-71.740723,42.0,-46.494141,56.161294)
}

lat_clip = 1.5
long_clip = 3
lat_feature_clip = 5
long_feature_clip = 7.5

lines_df = gpd.read_file("../10m_physical/" + "gebco_contours4osm.shp", engine='pyogrio')
oceans_df = gpd.read_file("../10m_physical/" + "ne_10m_ocean_scale_rank.shp", engine='pyogrio')
countries_df = gpd.read_file("../10m_physical/" + "ne_10m_admin_0_countries.shp", engine='pyogrio')

for key, bbox in bboxs.items():
    bath_bbox = (bbox[0] - long_clip, bbox[1] - lat_clip, bbox[2] + long_clip, bbox[3] + lat_clip)
    feature_bbox = (bbox[0] - long_feature_clip, bbox[1] - lat_feature_clip, bbox[2] + long_feature_clip, bbox[3] + lat_feature_clip)
    
    lines_df_clipped = gpd.clip(lines_df, bath_bbox)

    oceans_df_clipped = gpd.clip(oceans_df, feature_bbox)

    countries_df_clipped = gpd.clip(countries_df, feature_bbox)

    lines_df_clipped.to_file("./Map_Imports/Bathymetry_{}.shp".format(key))

    oceans_df_clipped.to_file("./Map_Imports/Ocean_{}.shp".format(key))

    countries_df_clipped.to_file("./Map_Imports/Countries_{}.shp".format(key))

    print("Generated:")
    print("\t", "./Map_Imports/Bathymetry_{}.shp".format(key))
    print("\t", "./Map_Imports/Ocean_{}.shp".format(key))
    print("\t", "./Map_Imports/Countries_{}.shp".format(key))
