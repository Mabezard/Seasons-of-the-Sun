# Seasons-of-the-Sun
Dynamically calculates Skyrim's sun height angle based on a latitude and the date for better seasonal immersion.

Players choose a latitude in an MCM:
-Northern Europe 53N
-United States 39N
-Australia 27S
-New Zeland 41S
-or any custom latitude between 65N to 65S

Optional compatibility with "Four Seasons - Faster Seasons of Skyrim"

The calculated angle varies +/-23 degrees around the chosen Latitude throughout the year. Low Sun and long shadows in the winter and high sun with short shadows in the Summer. The angle is written to an override json for the SkySync feature of Community Shaders.

Seasons of the Sun also dynamically updates seasonal sunrise and sunset times each day, ensuring Vampire conditionals react to the changing daylight hours.

An SKSE mod built using CommonLib and help from Gemini
