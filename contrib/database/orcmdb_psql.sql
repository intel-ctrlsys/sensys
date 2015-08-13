--
-- Copyright (c) 2014-2015 Intel Inc. All rights reserved
--

--
-- PostgreSQL database dump
--

SET statement_timeout = 0;
SET lock_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

--
-- Name: plpgsql; Type: EXTENSION; Schema: -; Owner: -
--

--CREATE EXTENSION IF NOT EXISTS plpgsql WITH SCHEMA pg_catalog;


--
-- Name: EXTENSION plpgsql; Type: COMMENT; Schema: -; Owner: -
--

--COMMENT ON EXTENSION plpgsql IS 'PL/pgSQL procedural language';


SET search_path = public, pg_catalog;

--
-- Name: add_data_item(character varying, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_data_item(p_name character varying, p_data_type_id integer) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_data_item_id integer := 0;

BEGIN
    insert into data_item(
        name,
        data_type_id)
    values(
        p_name,
        p_data_type_id);

    select data_item_id into v_data_item_id
    from data_item
    where name = p_name;

    return v_data_item_id;
END

$$;


--
-- Name: add_data_sample(integer, integer, timestamp without time zone, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_data_sample(p_node_id integer, p_data_item_id integer, p_time_stamp timestamp without time zone, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    insert into data_sample(
        node_id,
        data_item_id,
        time_stamp,
        value_int,
        value_real,
        value_str,
        units)
    values(
        p_node_id,
        p_data_item_id,
        p_time_stamp,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units);

    return;
END
$$;


--
-- Name: add_data_type(integer, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_data_type(p_data_type_id integer, p_name character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    insert into data_type(
        data_type_id,
        name)
    values(
        p_data_type_id,
        p_name);
END
$$;


--
-- Name: add_diag(integer, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag(p_diag_type_id integer, p_diag_subtype_id integer) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    insert into diag(
        diag_type_id,
        diag_subtype_id)
    values(
        p_diag_type_id,
        p_diag_subtype_id);

    return;
END
$$;


--
-- Name: add_diag_subtype(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag_subtype(p_name character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_diag_subtype_id integer := 0;

BEGIN
    insert into diag_subtype(name)
    values(p_name);

    select diag_subtype_id into v_diag_subtype_id
    from diag_subtype
    where name = p_name;

    return v_diag_subtype_id;
END

$$;


--
-- Name: add_diag_test(integer, integer, integer, timestamp without time zone, timestamp without time zone, integer, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag_test(p_node_id integer, p_diag_type_id integer, p_diag_subtype_id integer, p_start_time timestamp without time zone, p_end_time timestamp without time zone, p_component_index integer, p_test_result_id integer) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    insert into diag_test(
        node_id,
        diag_type_id,
        diag_subtype_id,
        start_time,
        end_time,
        component_index,
        test_result_id)
    values(
        p_node_id,
        p_diag_type_id,
        p_diag_subtype_id,
        p_start_time,
        p_end_time,
        p_component_index,
        p_test_result_id);

    return;
END
$$;


--
-- Name: add_diag_test_config(integer, integer, integer, timestamp without time zone, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag_test_config(p_node_id integer, p_diag_type_id integer, p_diag_subtype_id integer, p_start_time timestamp without time zone, p_test_param_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    insert into diag_test_config(
        node_id,
        diag_type_id,
        diag_subtype_id,
        start_time,
        test_param_id,
        value_int,
        value_real,
        value_str,
        units)
    values(
        p_node_id,
        p_diag_type_id,
        p_diag_subtype_id,
        p_start_time,
        p_test_param_id,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units);

    return;
END
$$;


--
-- Name: add_diag_type(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag_type(p_name character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_diag_type_id integer := 0;

BEGIN
    insert into diag_type(name)
    values(p_name);

    select diag_type_id into v_diag_type_id
    from diag_type
    where name = p_name;

    return v_diag_type_id;
END

$$;


--
-- Name: add_feature(character varying, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_feature(p_name character varying, p_data_type_id integer) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_feature_id integer := 0;

BEGIN
    insert into feature(
        name,
        data_type_id)
    values(
        p_name,
        p_data_type_id);

    select feature_id into v_feature_id
    from feature
    where name = p_name;

    return v_feature_id;
END

$$;


--
-- Name: add_node(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_node(p_hostname character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_node_id integer := 0;

BEGIN
    insert into node(hostname)
    values(p_hostname);

    select node_id into v_node_id
    from node
    where hostname = p_hostname;

    return v_node_id;
END

$$;


--
-- Name: add_node_feature(integer, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_node_feature(p_node_id integer, p_feature_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    insert into node_feature(
        node_id,
        feature_id,
        value_int,
        value_real,
        value_str,
        units)
    values(
        p_node_id,
        p_feature_id,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units);

    return;
END
$$;


--
-- Name: add_test_param(character varying, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_test_param(p_name character varying, p_data_type_id integer) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_test_param_id integer := 0;

BEGIN
    insert into test_param(
        name,
        data_type_id)
    values(
        p_name,
        p_data_type_id);

    select test_param_id into v_test_param_id
    from test_param
    where name = p_name;

    return v_test_param_id;
END
$$;


--
-- Name: add_test_result(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_test_result(p_name character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_test_result_id integer := 0;

BEGIN
    insert into test_result(name)
    values(p_name);

    select test_result_id into v_test_result_id
    from test_result
    where name = p_name;

    return v_test_result_id;
END
$$;


--
-- Name: alert_db_size(double precision, double precision, double precision, double precision, text); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION alert_db_size(
    max_db_size_in_gb DOUBLE PRECISION,
    log_level_percentage DOUBLE PRECISION,
    warning_level_percentage DOUBLE PRECISION,
    critical_level_percentage DOUBLE PRECISION,
    log_tag TEXT)
  RETURNS TEXT
  LANGUAGE plpgsql VOLATILE
AS
$BODY$
-- Query the current database for its size and compare with the
-- specified arguments to raise the corresponding message.
-- Parameter size restriction:
--     0 < max_db_size_in_gb
--     0 <= notice < warning < critical <= 1.0
-- Note:  Enable "log_destination='syslog'" in postgresql.conf to log to syslog
DECLARE
    actual_db_size BIGINT;
    max_db_size BIGINT;
    db_log_size BIGINT;
    db_warning_size BIGINT;
    db_critical_size BIGINT;
    message_prefix TEXT;
BEGIN
    -- Input validation
    IF (0 >= max_db_size_in_gb) THEN
        RAISE EXCEPTION '[%] The maximum database size must be greater than 0.0 GB', log_tag USING HINT = 'Invalid argument';
    END IF;
    IF (0 > critical_level_percentage OR critical_level_percentage > 1) THEN
        RAISE EXCEPTION '[%] The percentage values must be between 0.0 and 1.0', log_tag USING HINT = 'Invalid  argument';
    END IF;
    IF (warning_level_percentage >= critical_level_percentage) THEN
        RAISE EXCEPTION '[%] The WARNING level percentage must be less than the CRITICAL level percentage', log_tag USING HINT = 'Invalid  argument';
    END IF;
    IF (log_level_percentage >= warning_level_percentage) THEN
        RAISE EXCEPTION '[%] The LOG level percentage must be less than the WARNING level percentage', log_tag USING HINT = 'Invalid  argument';
    END IF;
    -- Check permission
    IF NOT (pg_catalog.has_database_privilege(current_database(), 'CONNECT')) THEN
        RAISE EXCEPTION '[%] User ''%'' does not have permission to query ''%'' database size', log_tag, CURRENT_USER, current_database();
    END IF;

    -- Convert size to byte for finer comparison
    max_db_size      = CAST(max_db_size_in_gb * 1024 * 1024 * 1024 AS BIGINT);
    db_log_size      = CAST(max_db_size * log_level_percentage AS BIGINT);
    db_warning_size  = CAST(max_db_size * warning_level_percentage AS BIGINT);
    db_critical_size = CAST(max_db_size * critical_level_percentage AS BIGINT);

    SELECT
        pg_catalog.pg_database_size(d.datname)
    INTO actual_db_size
    FROM pg_catalog.pg_database d
    WHERE d.datname = current_database();

    -- Text to be included in all output
    message_prefix = '[' || log_tag || '] The ''' || current_database() || ''' database size (' || pg_catalog.pg_size_pretty(actual_db_size) || ')';

    IF (actual_db_size > db_critical_size) THEN
        RAISE EXCEPTION   '% exceeded critical level (% %% = %). ', message_prefix, 100 * critical_level_percentage,  pg_catalog.pg_size_pretty(db_critical_size) ;
    ELSEIF (actual_db_size > db_warning_size) THEN
        RAISE WARNING  '% exceeded warning level (% %% = %). ', message_prefix, 100 * warning_level_percentage,  pg_catalog.pg_size_pretty(db_warning_size) ;
        RETURN message_prefix || ' exceeded warning level (' || (100 * warning_level_percentage) || '% = ' || pg_catalog.pg_size_pretty(db_warning_size) || ').';
    ELSEIF (actual_db_size > db_log_size) THEN
        RAISE LOG   '% exceeded log level (% %% = %). ', message_prefix, 100 * log_level_percentage,  pg_catalog.pg_size_pretty(db_log_size) ;
        RETURN message_prefix || ' exceeded log level (' || (100 * log_level_percentage) || '% = ' || pg_catalog.pg_size_pretty(db_log_size) || ').';
    ELSE
        RETURN message_prefix || ' is within limit (' || ROUND( CAST((100 * CAST(actual_db_size AS DOUBLE PRECISION) / max_db_size) AS NUMERIC), 2) || '% of ' || pg_catalog.pg_size_pretty(max_db_size) || ')';
    END IF;
END;
$BODY$;


--
-- Name: data_type_exists(integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION data_type_exists(p_data_type_id integer) RETURNS boolean
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_exists boolean := false;

BEGIN
    v_exists := exists(
        select 1
        from data_type
        where data_type_id = p_data_type_id);

    return v_exists;
END
$$;


--
-- Name: diag_exists(character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION diag_exists(p_diag_type character varying, p_diag_subtype character varying) RETURNS boolean
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_exists boolean := false;
    v_diag_type_id integer := 0;
    v_diag_subtype_id integer := 0;

BEGIN
    -- Get the diagnostic type ID
    v_diag_type_id := get_diag_type_id(p_diag_type);
    if (v_diag_type_id = 0) then
        -- If it doesn't exist, the diagnostic doesn't exist either
        return false;
    end if;

    -- Get the diagnostic subtype ID
    v_diag_subtype_id := get_diag_subtype_id(p_diag_subtype);
    if (v_diag_subtype_id = 0) then
        -- If it doesn't exist, the diagnostic doesn't exist either
        return false;
    end if;

    v_exists := exists(
        select 1
        from diag
        where diag_type_id = v_diag_type_id and
            diag_subtype_id = v_diag_subtype_id);

    return v_exists;
END
$$;


--
-- Name: diag_exists(integer, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION diag_exists(p_diag_type_id integer, p_diag_subtype_id integer) RETURNS boolean
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_exists boolean := false;

BEGIN
    v_exists := exists(
        select 1
        from diag
        where diag_type_id = p_diag_type_id and
            diag_subtype_id = p_diag_subtype_id);

    return v_exists;
END
$$;


--
-- Name: generate_partition_triggers_ddl(text, text, text, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION generate_partition_triggers_ddl(table_name text, time_stamp_column_name text, partition_interval text, number_of_interval_of_data_to_keep integer DEFAULT 180) RETURNS text
    LANGUAGE plpgsql IMMUTABLE STRICT
    AS $_$
DECLARE
	ddl_script text;
	timestamp_mask_by_interval text;
	insert_stmt text;
	table_name_from_arg ALIAS FOR table_name;
	insert_stmt_position integer;
	insert_stmt_column_name varchar;
BEGIN

-- Convert the interval to pattern to be used to name partitions
IF (partition_interval = 'YEAR') THEN
	timestamp_mask_by_interval := 'YYYY';
ELSEIF (partition_interval = 'MONTH') THEN
	timestamp_mask_by_interval := 'YYYYMM';
ELSEIF (partition_interval = 'DAY') THEN
	timestamp_mask_by_interval := 'YYYYMMDD';
ELSEIF (partition_interval = 'HOUR') THEN
	timestamp_mask_by_interval := 'YYYYMMDDHH24';
ELSEIF (partition_interval = 'MINUTE') THEN
	timestamp_mask_by_interval := 'YYYYMMDDHH24MI';
ELSE
	RAISE EXCEPTION 'The specified % is not supported interval.  The supported values are YEAR, MONTH, DAY, HOUR, MINUTE', quote_literal(partition_interval);
END IF;

-- Verify timestamp datatype
PERFORM 1 FROM information_schema.columns WHERE information_schema.columns.table_name = table_name_from_arg AND column_name = time_stamp_column_name AND data_type LIKE 'timestamp%';
IF (NOT FOUND) THEN
	RAISE EXCEPTION 'The specified column % in table % must exists and of type timestamp', quote_literal(time_stamp_column_name), quote_literal(table_name);
END IF;

-- Generate insert statement
insert_stmt := 'EXECUTE(''INSERT INTO '' || quote_ident(''' || table_name || '_'' || to_char(NEW.' || time_stamp_column_name || ', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') || ''(';

-- Building up the list of column names in the <table_name> (....) section of the INSERT statement
--     Handling the first column name in the list without the leading commas
SELECT column_name
	FROM information_schema.columns
	WHERE information_schema.columns.table_name = table_name_from_arg AND ordinal_position = 1
	ORDER BY ordinal_position
	INTO insert_stmt_column_name;
insert_stmt := insert_stmt || insert_stmt_column_name;
--     Handling the rest of the column names with commas separated
FOR insert_stmt_column_name IN SELECT column_name
	FROM information_schema.columns
	WHERE information_schema.columns.table_name = table_name_from_arg AND ordinal_position > 1
	ORDER BY ordinal_position
LOOP
	insert_stmt := insert_stmt || ', ' || insert_stmt_column_name;
END LOOP;
-- Building up the list of parameters position for dynamic EXECUTE statement
--     Handling the first parameter position without the leading commas
insert_stmt := insert_stmt || ') VALUES ($1';
--     Handling the rest of the parameter positions with commas separated
FOR insert_stmt_position IN SELECT ordinal_position
	FROM information_schema.columns
	WHERE information_schema.columns.table_name = table_name_from_arg AND ordinal_position > 1
	ORDER BY ordinal_position
LOOP
	insert_stmt := insert_stmt || ', $' || insert_stmt_position;
END LOOP;

insert_stmt := insert_stmt || ');'')
        USING ';
-- Building up a list of parameters to the dynamic EXECUTE statement
--     Handling the first column name in the list without the leading commas
SELECT 'NEW.' || column_name
	FROM information_schema.columns
	WHERE information_schema.columns.table_name = table_name_from_arg AND ordinal_position = 1
	ORDER BY ordinal_position
	INTO insert_stmt_column_name;
insert_stmt := insert_stmt || insert_stmt_column_name;
--     Handling the rest of the column names with commas separated
FOR insert_stmt_column_name IN SELECT column_name
	FROM information_schema.columns
	WHERE information_schema.columns.table_name = table_name_from_arg AND ordinal_position > 1
	ORDER BY ordinal_position
LOOP
	insert_stmt := insert_stmt || ', NEW.' || insert_stmt_column_name;
END LOOP;

insert_stmt := insert_stmt || ';';



ddl_script := '-- Review the generated code before invocation
-- Drop old partition is disabled by default.  Uncomment the code in the generated statement to enable auto drop partition.
-- Partition for ' || quote_literal(table_name) ||
' based on the timestamp value of column ' || quote_literal(time_stamp_column_name) ||
' using partition size for every one ' || quote_literal(partition_interval) || '.
-- New partition name pattern:  ' || quote_literal(table_name || '_' || timestamp_mask_by_interval || '_' || time_stamp_column_name) || '.' ||
'
-------------------------------
-- Function: ' || table_name || '_partition_handler()

-- DROP FUNCTION ' || table_name || '_partition_handler();

CREATE OR REPLACE FUNCTION ' || table_name || '_partition_handler()
  RETURNS trigger AS
$PARTITION_BODY$
BEGIN';

     PERFORM 1 FROM information_schema.columns WHERE information_schema.columns.table_name = table_name_from_arg AND column_name = time_stamp_column_name AND data_type LIKE 'timestamp%' AND is_nullable = 'YES';
     IF (FOUND) THEN
         ddl_script := ddl_script || '
         IF (NEW.' || time_stamp_column_name || ' IS NULL) THEN
             NEW.' || time_stamp_column_name || ' := now();
         END IF;
         ';
     END IF;

ddl_script := ddl_script || '
    PERFORM 1
            FROM information_schema.tables
            WHERE table_name = quote_ident(''' || table_name || '_'' || to_char(NEW.time_stamp, ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''');
    IF NOT FOUND THEN
        BEGIN
            -- Create the partition
            EXECUTE(''CREATE TABLE IF NOT EXISTS '' || quote_ident(''' || table_name || '_'' || to_char(NEW.' || time_stamp_column_name || ', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') ||
                        '' (CHECK ( ' || time_stamp_column_name || ' >= date_trunc(''''' || partition_interval || ''''', timestamp '''''' || NEW.' || time_stamp_column_name || ' || '''''') '' ||
                        ''      AND ' || time_stamp_column_name || ' <  date_trunc(''''' || partition_interval || ''''', timestamp '''''' || NEW.' || time_stamp_column_name || ' || '''''' + interval ''''1 ' || partition_interval || ''''') ) ) '' ||
                        '' INHERITS (' || table_name || '); '');
            EXECUTE(''CREATE INDEX '' || quote_ident(''index_' || table_name || '_'' || to_char(NEW.' || time_stamp_column_name || ', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') ||
                        '' ON '' || quote_ident(''' || table_name || '_'' || to_char(NEW.' || time_stamp_column_name || ', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') ||
                        '' (' || time_stamp_column_name || '); '');
        EXCEPTION
            WHEN duplicate_table THEN
                -- This error also applies to duplicate index
                -- Ignore error
        END;
        --
        -- <Place holder for data archive statement to be execute before DROP partition>
        --
        -- Uncomment the drop table statement to not have partition be auto removed
        -- WARNING *** Non Recoverable once partition has been drop *** WARNING --
        -- EXECUTE(''DROP TABLE IF EXISTS '' || quote_ident(''' || table_name || '_'' || to_char(NEW.' || time_stamp_column_name || ' - interval ''' || number_of_interval_of_data_to_keep || ' ' || partition_interval || ''', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') || '';'');
        -- WARNING *** Non Recoverable once partition has been drop *** WARNING --
    END IF;

    ' || insert_stmt || '
    RETURN NULL;
END;
$PARTITION_BODY$
  LANGUAGE plpgsql VOLATILE;
-------------------------------


-------------------------------
-- Trigger: insert_' || table_name || '_trigger on ' || table_name || '

-- DROP TRIGGER insert_' || table_name || '_trigger ON ' || table_name || ';

';

-- Comment out the CREATE TRIGGER statement if the trigger already exists
PERFORM 1 FROM information_schema.triggers WHERE trigger_name = 'insert_' || table_name || '_trigger';
IF (FOUND) THEN
    ddl_script := ddl_script || '-- Trigger of the same name already exists.  Reminder to make sure it has the expected behavior.
-- ';
END IF;

ddl_script := ddl_script || 'CREATE TRIGGER insert_' || table_name || '_trigger BEFORE INSERT ON ' || table_name || ' FOR EACH ROW EXECUTE PROCEDURE ' || table_name || '_partition_handler();
-------------------------------
';

RETURN ddl_script;

END;
$_$;


--
-- Name: get_data_item_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_data_item_id(p_data_item character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_data_item_id integer := 0;

BEGIN
    -- Get the data item ID
    select data_item_id into v_data_item_id
    from data_item
    where name = p_data_item;

    -- Does it exist?
    if (v_data_item_id is null) then
        v_data_item_id := 0;
    end if;

    return v_data_item_id;
END
$$;


--
-- Name: get_diag_subtype_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_diag_subtype_id(p_diag_subtype character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_diag_subtype_id integer := 0;

BEGIN
    -- Get the diagnostic subtype
    select diag_subtype_id into v_diag_subtype_id
    from diag_subtype
    where name = p_diag_subtype;

    -- Does it exist?
    if (v_diag_subtype_id is null) then
        v_diag_subtype_id := 0;
    end if;

    return v_diag_subtype_id;
END
$$;


--
-- Name: get_diag_type_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_diag_type_id(p_diag_type character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_diag_type_id integer := 0;

BEGIN
    -- Get the diagnostic type
    select diag_type_id into v_diag_type_id
    from diag_type
    where name = p_diag_type;

    -- Does it exist?
    if (v_diag_type_id is null) then
        v_diag_type_id := 0;
    end if;

    return v_diag_type_id;
END
$$;


--
-- Name: get_feature_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_feature_id(p_feature character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_feature_id integer := 0;

BEGIN
    -- Get the feature ID
    select feature_id into v_feature_id
    from feature
    where name = p_feature;

    -- Does it exist?
    if (v_feature_id is null) then
        v_feature_id := 0;
    end if;

    return v_feature_id;
END
$$;


--
-- Name: get_node_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_node_id(p_hostname character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_node_id integer := 0;

BEGIN
    -- Get the node ID
    select node_id into v_node_id
    from node
    where hostname = p_hostname;

    -- Does it exist?
    if (v_node_id is null) then
        v_node_id := 0;
    end if;

    return v_node_id;
END

$$;


--
-- Name: get_test_param_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_test_param_id(p_name character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_test_param_id integer := 0;

BEGIN
    -- Get the test parameter ID
    select test_param_id into v_test_param_id
    from test_param
    where name = p_name;

    -- Does it exist?
    if (v_test_param_id is null) then
        v_test_param_id := 0;
    end if;

    return v_test_param_id;
END
$$;


--
-- Name: get_test_result_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_test_result_id(p_test_result character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_test_result_id integer := 0;

BEGIN
    -- Get the test result ID
    select test_result_id into v_test_result_id
    from test_result
    where name = p_test_result;

    -- Does it exist?
    if (v_test_result_id is null) then
        v_test_result_id := 0;
    end if;

    return v_test_result_id;
END
$$;


--
-- Name: node_feature_exists(integer, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION node_feature_exists(p_node_id integer, p_feature_id integer) RETURNS boolean
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_exists boolean := false;

BEGIN

    v_exists := exists(
        select 1
        from node_feature
        where node_id = p_node_id and feature_id = p_feature_id);

    return v_exists;
END
$$;


--
-- Name: purge_data(text, text, integer, text); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION purge_data(name_of_table text, name_of_timestamp_column text, number_of_time_unit_from_now integer, time_unit text) RETURNS void
  LANGUAGE plpgsql VOLATILE
  AS $$
BEGIN
    EXECUTE('DELETE FROM ' || name_of_table || ' WHERE ' || name_of_table || '.' || name_of_timestamp_column || ' < CURRENT_TIMESTAMP - interval ''' || number_of_time_unit_from_now || ' ' || time_unit || ''';');
END
$$;


--
-- Name: record_data_sample(character varying, character varying, character varying, timestamp without time zone, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION record_data_sample(p_hostname character varying, p_data_group character varying, p_data_item character varying, p_time_stamp timestamp without time zone, p_data_type_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id integer := 0;
    v_data_item_id integer := 0;
    v_data_item_name varchar(250);

BEGIN
    -- Compose the data item name from the data group and item itself
    v_data_item_name := p_data_group || '_';
    v_data_item_name := v_data_item_name || p_data_item;

    -- Insert in the data_sample_raw table
    insert into data_sample_raw(
        hostname,
        data_item,
        time_stamp,
        value_int,
        value_real,
        value_str,
        units,
        data_type_id)
    values(
        p_hostname,
        v_data_item_name,
        p_time_stamp,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units,
        p_data_type_id);

    -- Disable storing in data_sample table for now...
    -- Get the node ID
    -- v_node_id := get_node_id(p_hostname);
    --if (v_node_id = 0) then
    --    v_node_id := add_node(p_hostname);
    --end if;

    --if (not data_type_exists(p_data_type_id)) then
    --    perform add_data_type(p_data_type_id, NULL);
    --end if;

    -- Get the data item ID
    --v_data_item_id := get_data_item_id(v_data_item_name);
    --if (v_data_item_id = 0) then
    --    v_data_item_id := add_data_item(v_data_item_name, p_data_type_id);
    --end if;

    -- Add the data sample
    --perform add_data_sample(
    --    v_node_id,
    --    v_data_item_id,
    --    p_time_stamp,
    --    p_value_int,
    --    p_value_real,
    --    p_value_str,
    --    p_units);

    return;
END;
$$;


--
-- Name: record_diag_test_config(character varying, character varying, character varying, timestamp without time zone, character varying, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION record_diag_test_config(p_hostname character varying, p_diag_type character varying, p_diag_subtype character varying, p_start_time timestamp without time zone, p_test_param character varying, p_data_type_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id integer := 0;
    v_diag_type_id integer := 0;
    v_diag_subtype_id integer := 0;
    v_test_param_id integer := 0;

BEGIN
    --Get the node ID
    v_node_id := get_node_id(p_hostname);
    if (v_node_id = 0) then
        v_node_id := add_node(p_hostname);
    end if;

    -- Get the diagnostic type ID
    v_diag_type_id := get_diag_type_id(p_diag_type);
    if (v_diag_type_id = 0) then
        v_diag_type_id := add_diag_type(p_diag_type);
    end if;

    -- Get the diagnostic subtype ID
    v_diag_subtype_id := get_diag_subtype_id(p_diag_subtype);
    if (v_diag_subtype_id = 0) then
        v_diag_subtype_id := add_diag_subtype(p_diag_subtype);
    end if;

    if (not data_type_exists(p_data_type_id)) then
        perform add_data_type(p_data_type_id, NULL);
    end if;

    -- Get the test parameter ID
    v_test_param_id := get_test_param_id(p_test_param);
    if (v_test_param_id = 0) then
        v_test_param_id := add_test_param(p_test_param, p_data_type_id);
    end if;

    -- Add the test configuration parameter
    perform add_diag_test_config(
        v_node_id,
        v_diag_type_id,
        v_diag_subtype_id,
        p_start_time,
        v_test_param_id,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units);

    return;
END
$$;


--
-- Name: record_diag_test_result(character varying, character varying, character varying, timestamp without time zone, timestamp without time zone, integer, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION record_diag_test_result(p_hostname character varying, p_diag_type character varying, p_diag_subtype character varying, p_start_time timestamp without time zone, p_end_time timestamp without time zone, p_component_index integer, p_test_result character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id integer := 0;
    v_diag_type_id integer := 0;
    v_diag_subtype_id integer := 0;
    v_test_result_id integer := 0;

BEGIN
    -- Get the node ID
    v_node_id := get_node_id(p_hostname);
    if (v_node_id = 0) then
        v_node_id := add_node(p_hostname);
    end if;

    -- Get the diagnostic type ID
    v_diag_type_id := get_diag_type_id(p_diag_type);
    if (v_diag_type_id = 0) then
        v_diag_type_id := add_diag_type(p_diag_type);
    end if;

    -- Get the diagnostic subtype ID
    v_diag_subtype_id := get_diag_subtype_id(p_diag_subtype);
    if (v_diag_subtype_id = 0) then
        v_diag_subtype_id := add_diag_subtype(p_diag_subtype);
    end if;

    -- If this particular diagnostic does not exist, add it
    if (not diag_exists(v_diag_type_id, v_diag_subtype_id)) then
        perform add_diag(v_diag_type_id, v_diag_subtype_id);
    end if;

    -- Get the test result ID
    v_test_result_id := get_test_result_id(p_test_result);
    if (v_test_result_id = 0) then
        v_test_result_id := add_test_result(p_test_result);
    end if;

    -- Add the diagnostic test result
    perform add_diag_test(
        v_node_id,
        v_diag_type_id,
        v_diag_subtype_id,
        p_start_time,
        p_end_time,
        p_component_index,
        v_test_result_id);

    return;
END
$$;


--
-- Name: set_node_feature(character varying, character varying, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION set_node_feature(p_hostname character varying, p_feature character varying, p_data_type_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_node_id integer := 0;
    v_feature_id integer := 0;
    v_node_feature_exists boolean := true;

BEGIN
    -- Get the node ID
    v_node_id := get_node_id(p_hostname);
    if (v_node_id = 0) then
        v_node_id := add_node(p_hostname);
        v_node_feature_exists := false;
    end if;

    if (not data_type_exists(p_data_type_id)) then
        perform add_data_type(p_data_type_id, NULL);
    end if;

    -- Get the feature ID
    v_feature_id := get_feature_id(p_feature);
    if (v_feature_id = 0) then
        v_feature_id := add_feature(p_feature, p_data_type_id);
        v_node_feature_exists := false;
    end if;

    -- Has this feature already been added for this node?
    if (v_node_feature_exists) then
        v_node_feature_exists := node_feature_exists(v_node_id, v_feature_id);
    end if;

    if (v_node_feature_exists) then
        -- If so, just update it
        perform update_node_feature(
            v_node_id,
            v_feature_id,
            p_value_int,
            p_value_real,
            p_value_str,
            p_units);
    else        
        -- If not, add it
        perform add_node_feature(
            v_node_id,
            v_feature_id,
            p_value_int,
            p_value_real,
            p_value_str,
            p_units);
    end if;

    return;
END

$$;


--
-- Name: update_node_feature(integer, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION update_node_feature(p_node_id integer, p_feature_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    update node_feature
    set value_int = p_value_int,
        value_real = p_value_real,
        value_str = p_value_str,
        units = p_units
    where node_id = p_node_id and feature_id = p_feature_id;
END
$$;


SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: data_item; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE data_item (
    data_item_id integer NOT NULL,
    name character varying(250) NOT NULL,
    data_type_id integer NOT NULL
);


--
-- Name: data_item_data_item_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE data_item_data_item_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: data_item_data_item_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE data_item_data_item_id_seq OWNED BY data_item.data_item_id;


--
-- Name: data_sample; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE data_sample (
    node_id integer NOT NULL,
    data_item_id integer NOT NULL,
    time_stamp timestamp without time zone NOT NULL,
    value_int bigint,
    value_real double precision,
    value_str character varying(50),
    units character varying(50),
    CONSTRAINT data_sample_check CHECK ((((value_int IS NOT NULL) OR (value_real IS NOT NULL)) OR (value_str IS NOT NULL)))
);


--
-- Name: data_sample_raw; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE data_sample_raw (
    hostname character varying(256) NOT NULL,
    data_item character varying(250) NOT NULL,
    time_stamp timestamp without time zone NOT NULL,
    value_int bigint,
    value_real double precision,
    value_str character varying(50),
    units character varying(50),
    data_type_id integer NOT NULL,
    CONSTRAINT data_sample_raw_check CHECK ((((value_int IS NOT NULL) OR (value_real IS NOT NULL)) OR (value_str IS NOT NULL)))
);


--
-- Name: data_samples_view; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW data_samples_view AS
 SELECT data_sample_raw.hostname,
    data_sample_raw.data_item,
    data_sample_raw.data_type_id,
    data_sample_raw.time_stamp,
    data_sample_raw.value_int,
    data_sample_raw.value_real,
    data_sample_raw.value_str,
    data_sample_raw.units
   FROM data_sample_raw;


--
-- Name: data_type; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE data_type (
    data_type_id integer NOT NULL,
    name character varying(150)
);


--
-- Name: diag; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE diag (
    diag_type_id integer NOT NULL,
    diag_subtype_id integer NOT NULL
);


--
-- Name: diag_subtype; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE diag_subtype (
    diag_subtype_id integer NOT NULL,
    name character varying(250) NOT NULL
);


--
-- Name: diag_subtype_diag_subtype_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE diag_subtype_diag_subtype_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: diag_subtype_diag_subtype_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE diag_subtype_diag_subtype_id_seq OWNED BY diag_subtype.diag_subtype_id;


--
-- Name: diag_test; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE diag_test (
    node_id integer NOT NULL,
    diag_type_id integer NOT NULL,
    diag_subtype_id integer NOT NULL,
    start_time timestamp without time zone NOT NULL,
    end_time timestamp without time zone,
    component_index integer,
    test_result_id integer NOT NULL
);


--
-- Name: diag_test_config; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE diag_test_config (
    node_id integer NOT NULL,
    diag_type_id integer NOT NULL,
    diag_subtype_id integer NOT NULL,
    start_time timestamp without time zone NOT NULL,
    test_param_id integer NOT NULL,
    value_real double precision,
    value_str character varying(100),
    units character varying(50),
    value_int bigint,
    CONSTRAINT diag_test_config_check CHECK ((((value_int IS NOT NULL) OR (value_real IS NOT NULL)) OR (value_str IS NOT NULL)))
);


--
-- Name: test_param; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE test_param (
    test_param_id integer NOT NULL,
    name character varying(250) NOT NULL,
    data_type_id integer NOT NULL
);


--
-- Name: diag_test_param_diag_test_param_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE diag_test_param_diag_test_param_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: diag_test_param_diag_test_param_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE diag_test_param_diag_test_param_id_seq OWNED BY test_param.test_param_id;


--
-- Name: diag_type; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE diag_type (
    diag_type_id integer NOT NULL,
    name character varying(250) NOT NULL
);


--
-- Name: diag_type_diag_type_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE diag_type_diag_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: diag_type_diag_type_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE diag_type_diag_type_id_seq OWNED BY diag_type.diag_type_id;


--
-- Name: event; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE event (
    node_id integer NOT NULL,
    event_type_id integer NOT NULL,
    time_stamp timestamp without time zone NOT NULL,
    description text NOT NULL
);


--
-- Name: event_type; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE event_type (
    event_type_id integer NOT NULL,
    name character varying(250)
);


--
-- Name: event_type_event_type_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE event_type_event_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: event_type_event_type_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE event_type_event_type_id_seq OWNED BY event_type.event_type_id;


--
-- Name: feature; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE feature (
    feature_id integer NOT NULL,
    name character varying(250) NOT NULL,
    data_type_id integer NOT NULL
);


--
-- Name: feature_feature_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE feature_feature_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: feature_feature_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE feature_feature_id_seq OWNED BY feature.feature_id;


--
-- Name: fru; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE fru (
    node_id integer NOT NULL,
    fru_type_id integer NOT NULL,
    fru_id integer NOT NULL,
    serial_number character varying(50) NOT NULL
);


--
-- Name: fru_fru_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE fru_fru_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: fru_fru_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE fru_fru_id_seq OWNED BY fru.fru_id;


--
-- Name: fru_type; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE fru_type (
    fru_type_id integer NOT NULL,
    name character varying(250) NOT NULL
);


--
-- Name: fru_type_fru_type_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE fru_type_fru_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: fru_type_fru_type_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE fru_type_fru_type_id_seq OWNED BY fru_type.fru_type_id;


--
-- Name: job; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE job (
    job_id integer NOT NULL,
    name character varying(100),
    num_nodes integer NOT NULL,
    username character varying(75) NOT NULL,
    time_submitted timestamp without time zone NOT NULL,
    start_time timestamp without time zone,
    end_time timestamp without time zone,
    energy_usage real,
    exit_status integer
);


--
-- Name: job_job_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE job_job_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: job_job_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE job_job_id_seq OWNED BY job.job_id;


--
-- Name: job_node; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE job_node (
    job_id integer NOT NULL,
    node_id integer NOT NULL
);


--
-- Name: maintenance_record; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE maintenance_record (
    node_id integer NOT NULL,
    fru_type_id integer NOT NULL,
    fru_id integer NOT NULL,
    replacement_date date NOT NULL,
    old_serial_number character varying(50) NOT NULL,
    new_serial_number character varying(50) NOT NULL
);


--
-- Name: node; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE node (
    node_id integer NOT NULL,
    hostname character varying(256) NOT NULL,
    node_type smallint,
    status smallint,
    num_cpus smallint,
    num_sockets smallint,
    cores_per_socket smallint,
    threads_per_core smallint,
    memory smallint
);


--
-- Name: node_feature; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE node_feature (
    node_id integer NOT NULL,
    feature_id integer NOT NULL,
    value_real double precision,
    value_str character varying(100),
    units character varying(50),
    value_int bigint,
    CONSTRAINT node_feature_check CHECK ((((value_int IS NOT NULL) OR (value_real IS NOT NULL)) OR (value_str IS NOT NULL)))
);


--
-- Name: node_features_view; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW node_features_view AS
 SELECT node_feature.node_id,
    node.hostname,
    node_feature.feature_id,
    feature.name AS feature,
    node_feature.value_int,
    node_feature.value_real,
    node_feature.value_str,
    node_feature.units
   FROM ((node_feature
     JOIN node ON ((node.node_id = node_feature.node_id)))
     JOIN feature ON ((feature.feature_id = node_feature.feature_id)));


--
-- Name: node_node_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE node_node_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: node_node_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE node_node_id_seq OWNED BY node.node_id;


--
-- Name: test_result; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE test_result (
    test_result_id integer NOT NULL,
    name character varying(250) NOT NULL
);


--
-- Name: test_result_test_result_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE test_result_test_result_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: test_result_test_result_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE test_result_test_result_id_seq OWNED BY test_result.test_result_id;


--
-- Name: data_item_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_item ALTER COLUMN data_item_id SET DEFAULT nextval('data_item_data_item_id_seq'::regclass);


--
-- Name: diag_subtype_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_subtype ALTER COLUMN diag_subtype_id SET DEFAULT nextval('diag_subtype_diag_subtype_id_seq'::regclass);


--
-- Name: diag_type_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_type ALTER COLUMN diag_type_id SET DEFAULT nextval('diag_type_diag_type_id_seq'::regclass);


--
-- Name: event_type_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY event_type ALTER COLUMN event_type_id SET DEFAULT nextval('event_type_event_type_id_seq'::regclass);


--
-- Name: feature_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY feature ALTER COLUMN feature_id SET DEFAULT nextval('feature_feature_id_seq'::regclass);


--
-- Name: fru_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY fru ALTER COLUMN fru_id SET DEFAULT nextval('fru_fru_id_seq'::regclass);


--
-- Name: fru_type_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY fru_type ALTER COLUMN fru_type_id SET DEFAULT nextval('fru_type_fru_type_id_seq'::regclass);


--
-- Name: job_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY job ALTER COLUMN job_id SET DEFAULT nextval('job_job_id_seq'::regclass);


--
-- Name: node_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY node ALTER COLUMN node_id SET DEFAULT nextval('node_node_id_seq'::regclass);


--
-- Name: test_param_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY test_param ALTER COLUMN test_param_id SET DEFAULT nextval('diag_test_param_diag_test_param_id_seq'::regclass);


--
-- Name: test_result_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY test_result ALTER COLUMN test_result_id SET DEFAULT nextval('test_result_test_result_id_seq'::regclass);


--
-- Name: data_item_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_item
    ADD CONSTRAINT data_item_pkey PRIMARY KEY (data_item_id);


--
-- Name: data_sample_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_sample
    ADD CONSTRAINT data_sample_pkey PRIMARY KEY (node_id, data_item_id, time_stamp);


--
-- Name: data_type_name_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_type
    ADD CONSTRAINT data_type_name_key UNIQUE (name);


--
-- Name: data_type_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_type
    ADD CONSTRAINT data_type_pkey PRIMARY KEY (data_type_id);


--
-- Name: diag_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag
    ADD CONSTRAINT diag_pkey PRIMARY KEY (diag_type_id, diag_subtype_id);


--
-- Name: diag_subtype_name_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_subtype
    ADD CONSTRAINT diag_subtype_name_key UNIQUE (name);


--
-- Name: diag_subtype_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_subtype
    ADD CONSTRAINT diag_subtype_pkey PRIMARY KEY (diag_subtype_id);


--
-- Name: diag_test_config_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_test_config
    ADD CONSTRAINT diag_test_config_pkey PRIMARY KEY (node_id, diag_type_id, diag_subtype_id, start_time, test_param_id);


--
-- Name: diag_test_param_name_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY test_param
    ADD CONSTRAINT diag_test_param_name_key UNIQUE (name);


--
-- Name: diag_test_param_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY test_param
    ADD CONSTRAINT diag_test_param_pkey PRIMARY KEY (test_param_id);


--
-- Name: diag_test_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_test
    ADD CONSTRAINT diag_test_pkey PRIMARY KEY (node_id, diag_type_id, diag_subtype_id, start_time);


--
-- Name: diag_type_name_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_type
    ADD CONSTRAINT diag_type_name_key UNIQUE (name);


--
-- Name: diag_type_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_type
    ADD CONSTRAINT diag_type_pkey PRIMARY KEY (diag_type_id);


--
-- Name: event_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY event
    ADD CONSTRAINT event_pkey PRIMARY KEY (node_id, event_type_id, time_stamp);


--
-- Name: event_type_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY event_type
    ADD CONSTRAINT event_type_pkey PRIMARY KEY (event_type_id);


--
-- Name: feature_name_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY feature
    ADD CONSTRAINT feature_name_key UNIQUE (name);


--
-- Name: feature_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY feature
    ADD CONSTRAINT feature_pkey PRIMARY KEY (feature_id);


--
-- Name: fru_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY fru
    ADD CONSTRAINT fru_pkey PRIMARY KEY (node_id, fru_type_id, fru_id);


--
-- Name: fru_type_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY fru_type
    ADD CONSTRAINT fru_type_pkey PRIMARY KEY (fru_type_id);


--
-- Name: job_node_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY job_node
    ADD CONSTRAINT job_node_pkey PRIMARY KEY (job_id, node_id);


--
-- Name: job_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY job
    ADD CONSTRAINT job_pkey PRIMARY KEY (job_id);


--
-- Name: maintenance_record_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY maintenance_record
    ADD CONSTRAINT maintenance_record_pkey PRIMARY KEY (node_id, fru_type_id, fru_id, replacement_date);


--
-- Name: node_feature_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY node_feature
    ADD CONSTRAINT node_feature_pkey PRIMARY KEY (node_id, feature_id);


--
-- Name: node_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY node
    ADD CONSTRAINT node_pkey PRIMARY KEY (node_id);


--
-- Name: test_result_name_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY test_result
    ADD CONSTRAINT test_result_name_key UNIQUE (name);


--
-- Name: test_result_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY test_result
    ADD CONSTRAINT test_result_pkey PRIMARY KEY (test_result_id);


--
-- Name: unique_host; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY node
    ADD CONSTRAINT unique_host UNIQUE (hostname);


--
-- Name: unique_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_item
    ADD CONSTRAINT unique_name UNIQUE (name);


--
-- Name: data_item_data_type_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_item
    ADD CONSTRAINT data_item_data_type_id_fkey FOREIGN KEY (data_type_id) REFERENCES data_type(data_type_id);


--
-- Name: data_sample_data_item_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_sample
    ADD CONSTRAINT data_sample_data_item_id_fkey FOREIGN KEY (data_item_id) REFERENCES data_item(data_item_id);


--
-- Name: data_sample_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_sample
    ADD CONSTRAINT data_sample_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: diag_diag_subtype_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag
    ADD CONSTRAINT diag_diag_subtype_id_fkey FOREIGN KEY (diag_subtype_id) REFERENCES diag_subtype(diag_subtype_id);


--
-- Name: diag_diag_type_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag
    ADD CONSTRAINT diag_diag_type_id_fkey FOREIGN KEY (diag_type_id) REFERENCES diag_type(diag_type_id);


--
-- Name: diag_test_config_diag_test_param_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_test_config
    ADD CONSTRAINT diag_test_config_diag_test_param_id_fkey FOREIGN KEY (test_param_id) REFERENCES test_param(test_param_id);


--
-- Name: diag_test_config_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_test_config
    ADD CONSTRAINT diag_test_config_node_id_fkey FOREIGN KEY (node_id, diag_type_id, diag_subtype_id, start_time) REFERENCES diag_test(node_id, diag_type_id, diag_subtype_id, start_time);


--
-- Name: diag_test_diag_type_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_test
    ADD CONSTRAINT diag_test_diag_type_id_fkey FOREIGN KEY (diag_type_id, diag_subtype_id) REFERENCES diag(diag_type_id, diag_subtype_id);


--
-- Name: diag_test_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_test
    ADD CONSTRAINT diag_test_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: event_event_type_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY event
    ADD CONSTRAINT event_event_type_id_fkey FOREIGN KEY (event_type_id) REFERENCES event_type(event_type_id);


--
-- Name: event_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY event
    ADD CONSTRAINT event_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: feature_data_type_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY feature
    ADD CONSTRAINT feature_data_type_id_fkey FOREIGN KEY (data_type_id) REFERENCES data_type(data_type_id);


--
-- Name: fru_fru_type_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY fru
    ADD CONSTRAINT fru_fru_type_id_fkey FOREIGN KEY (fru_type_id) REFERENCES fru_type(fru_type_id);


--
-- Name: fru_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY fru
    ADD CONSTRAINT fru_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: job_node_job_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY job_node
    ADD CONSTRAINT job_node_job_id_fkey FOREIGN KEY (job_id) REFERENCES job(job_id);


--
-- Name: job_node_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY job_node
    ADD CONSTRAINT job_node_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: maintenance_record_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY maintenance_record
    ADD CONSTRAINT maintenance_record_node_id_fkey FOREIGN KEY (node_id, fru_type_id, fru_id) REFERENCES fru(node_id, fru_type_id, fru_id);


--
-- Name: node_feature_feature_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY node_feature
    ADD CONSTRAINT node_feature_feature_id_fkey FOREIGN KEY (feature_id) REFERENCES feature(feature_id);


--
-- Name: node_feature_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY node_feature
    ADD CONSTRAINT node_feature_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: test_param_data_type_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY test_param
    ADD CONSTRAINT test_param_data_type_id_fkey FOREIGN KEY (data_type_id) REFERENCES data_type(data_type_id);


--
-- Name: public; Type: ACL; Schema: -; Owner: -
--

--REVOKE ALL ON SCHEMA public FROM PUBLIC;
--REVOKE ALL ON SCHEMA public FROM postgres;
--GRANT ALL ON SCHEMA public TO postgres;
--GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- PostgreSQL database dump complete
--

