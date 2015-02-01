PRAGMA journal_mode = PERSIST;

CREATE TABLE IF NOT EXISTS events (
	object INT,
	action INT,
	time_stamp BIGINT,
	app_id INT,
	info TEXT,
	id INTEGER PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS applications (
	name TEXT,
	id INTEGER PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS device_working_time (
	time_stamp BIGINT,
	device INT,
	time INT,
	id INTEGER PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS power_mode (
	time_stamp BIGINT,
	old_mode_number INT,
	new_mode_number INT,
	duration INT,
	battery_level_change INT,
	id INTEGER PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS proc_power_cons (
	appid TEXT,
	power_cons BIGINT,
	duration INT,
	day INT,
	PRIMARY KEY(appid, day)
);
