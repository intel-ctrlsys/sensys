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

CREATE EXTENSION IF NOT EXISTS plpgsql WITH SCHEMA pg_catalog;


--
-- Name: EXTENSION plpgsql; Type: COMMENT; Schema: -; Owner: -
--

COMMENT ON EXTENSION plpgsql IS 'PL/pgSQL procedural language';


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
    INSERT INTO data_item(
        name,
        data_type_id)
    VALUES(
        p_name,
        p_data_type_id);

    SELECT data_item_id INTO v_data_item_id
    FROM data_item
    WHERE name = p_name;

    RETURN v_data_item_id;
END
$$;


--
-- Name: add_data_sample(integer, integer, timestamp without time zone, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_data_sample(p_node_id integer, p_data_item_id integer, p_time_stamp timestamp without time zone, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    INSERT INTO data_sample(
        node_id,
        data_item_id,
        time_stamp,
        value_int,
        value_real,
        value_str,
        units)
    VALUES(
        p_node_id,
        p_data_item_id,
        p_time_stamp,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units);

    RETURN;
END
$$;


--
-- Name: add_data_type(integer, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_data_type(p_data_type_id integer, p_name character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    INSERT INTO data_type(
        data_type_id,
        name)
    VALUES(
        p_data_type_id,
        p_name);

    RETURN;
END
$$;


--
-- Name: add_diag(integer, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag(p_diag_type_id integer, p_diag_subtype_id integer) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    INSERT INTO diag(
        diag_type_id,
        diag_subtype_id)
    VALUES(
        p_diag_type_id,
        p_diag_subtype_id);

    RETURN;
END
$$;


--
-- Name: add_diag_subtype(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag_subtype(p_name character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_diag_subtype_id INTEGER := 0;
BEGIN
    INSERT INTO diag_subtype(name)
    VALUES(p_name);

    SELECT diag_subtype_id INTO v_diag_subtype_id
    FROM diag_subtype
    WHERE name = p_name;

    RETURN v_diag_subtype_id;
END
$$;


--
-- Name: add_diag_test(integer, integer, integer, timestamp without time zone, timestamp without time zone, integer, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag_test(p_node_id integer, p_diag_type_id integer, p_diag_subtype_id integer, p_start_time timestamp without time zone, p_end_time timestamp without time zone, p_component_index integer, p_test_result_id integer) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    INSERT INTO diag_test(
        node_id,
        diag_type_id,
        diag_subtype_id,
        start_time,
        end_time,
        component_index,
        test_result_id)
    VALUES(
        p_node_id,
        p_diag_type_id,
        p_diag_subtype_id,
        p_start_time,
        p_end_time,
        p_component_index,
        p_test_result_id);

    RETURN;
END
$$;


--
-- Name: add_diag_test_config(integer, integer, integer, timestamp without time zone, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag_test_config(p_node_id integer, p_diag_type_id integer, p_diag_subtype_id integer, p_start_time timestamp without time zone, p_test_param_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    INSERT INTO diag_test_config(
        node_id,
        diag_type_id,
        diag_subtype_id,
        start_time,
        test_param_id,
        value_int,
        value_real,
        value_str,
        units)
    VALUES(
        p_node_id,
        p_diag_type_id,
        p_diag_subtype_id,
        p_start_time,
        p_test_param_id,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units);

    RETURN;
END
$$;


--
-- Name: add_diag_type(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_diag_type(p_name character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_diag_type_id INTEGER := 0;
BEGIN
    INSERT INTO diag_type(name)
    VALUES(p_name);

    SELECT diag_type_id INTO v_diag_type_id
    FROM diag_type
    WHERE name = p_name;

    RETURN v_diag_type_id;
END
$$;


--
-- Name: add_event(timestamp without time zone, character varying, character varying, character varying, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_event(p_time_stamp timestamp without time zone, p_severity character varying, p_type character varying, p_version character varying, p_vendor character varying, p_description character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_event_id INTEGER := 0;
BEGIN
    INSERT INTO event(
        time_stamp,
        severity,
        type,
        version,
        vendor,
        description)
    VALUES(
        p_time_stamp,
        p_severity,
        p_type,
        p_version,
        p_vendor,
        p_description)
    RETURNING event_id INTO v_event_id;

    RETURN v_event_id;
END
$$;


--
-- Name: add_event_data(integer, character varying, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_event_data(p_event_id integer, p_key_name character varying, p_app_value_type_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_event_data_key_id INTEGER := 0;
BEGIN
    SELECT event_data_key_id INTO v_event_data_key_id
        FROM event_data_key
        WHERE name = p_key_name;

    IF (v_event_data_key_id IS NULL) THEN
        INSERT INTO event_data_key (name, app_value_type_id)
            VALUES (p_key_name, p_app_value_type_id)
            RETURNING event_data_key_id INTO v_event_data_key_id;
    END IF;

    INSERT INTO event_data(
        event_id,
        event_data_key_id,
        value_int,
        value_real,
        value_str,
        units)
    VALUES(
        p_event_id,
        v_event_data_key_id,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units);
    RETURN;
END
$$;


--
-- Name: add_feature(character varying, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_feature(p_name character varying, p_data_type_id integer) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_feature_id INTEGER := 0;
BEGIN
    INSERT INTO feature(
        name,
        data_type_id)
    VALUES(
        p_name,
        p_data_type_id);

    SELECT feature_id INTO v_feature_id
    FROM feature
    WHERE name = p_name;

    RETURN v_feature_id;
END
$$;


--
-- Name: add_node(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_node(p_hostname character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id INTEGER := 0;
BEGIN
    INSERT INTO node(hostname)
    VALUES(p_hostname);

    SELECT node_id INTO v_node_id
    FROM node
    WHERE hostname = p_hostname;

    RETURN v_node_id;
END

$$;


--
-- Name: add_node_feature(integer, integer, bigint, double precision, character varying, character varying, timestamp without time zone); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_node_feature(p_node_id integer, p_feature_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying, p_time_stamp timestamp without time zone) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    IF (p_time_stamp IS NULL) THEN
        SELECT CURRENT_TIMESTAMP INTO p_time_stamp;
    END IF;
    INSERT INTO node_feature(
        node_id,
        feature_id,
        value_int,
        value_real,
        value_str,
        units,
        time_stamp)
    VALUES(
        p_node_id,
        p_feature_id,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units,
        p_time_stamp);

    RETURN;
END
$$;


--
-- Name: add_test_param(character varying, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_test_param(p_name character varying, p_data_type_id integer) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_test_param_id INTEGER := 0;
BEGIN
    INSERT INTO test_param(
        name,
        data_type_id)
    VALUES(
        p_name,
        p_data_type_id);

    SELECT test_param_id INTO v_test_param_id
    FROM test_param
    WHERE name = p_name;

    RETURN v_test_param_id;
END
$$;


--
-- Name: add_test_result(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_test_result(p_name character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_test_result_id INTEGER := 0;
BEGIN
    INSERT INTO test_result(name)
    VALUES(p_name);

    SELECT test_result_id INTO v_test_result_id
    FROM test_result
    WHERE name = p_name;

    RETURN v_test_result_id;
END
$$;


--
-- Name: alert_db_size(double precision, double precision, double precision, double precision, text); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION alert_db_size(max_db_size_in_gb double precision, log_level_percentage double precision, warning_level_percentage double precision, critical_level_percentage double precision, log_tag text) RETURNS text
    LANGUAGE plpgsql
    AS $$
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
END
$$;


--
-- Name: data_type_exists(integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION data_type_exists(p_data_type_id integer) RETURNS boolean
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_exists BOOLEAN := FALSE;
BEGIN
    v_exists := exists(
        SELECT 1
        FROM data_type
        WHERE data_type_id = p_data_type_id);

    RETURN v_exists;
END
$$;


--
-- Name: diag_exists(integer, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION diag_exists(p_diag_type_id integer, p_diag_subtype_id integer) RETURNS boolean
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_exists BOOLEAN := FALSE;
BEGIN
    v_exists := exists(
        SELECT 1
        FROM diag
        WHERE diag_type_id = p_diag_type_id AND
            diag_subtype_id = p_diag_subtype_id);

    RETURN v_exists;
END
$$;


--
-- Name: diag_exists(character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION diag_exists(p_diag_type character varying, p_diag_subtype character varying) RETURNS boolean
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_exists BOOLEAN := FALSE;
    v_diag_type_id INTEGER := 0;
    v_diag_subtype_id INTEGER := 0;
BEGIN
    -- Get the diagnostic type ID
    v_diag_type_id := get_diag_type_id(p_diag_type);
    IF (v_diag_type_id = 0) THEN
        -- If it doesn't exist, the diagnostic doesn't exist either
        RETURN FALSE;
    END IF;

    -- Get the diagnostic subtype ID
    v_diag_subtype_id := get_diag_subtype_id(p_diag_subtype);
    IF (v_diag_subtype_id = 0) THEN
        -- If it doesn't exist, the diagnostic doesn't exist either
        RETURN FALSE;
    end IF;

    v_exists := exists(
        SELECT 1
        FROM diag
        WHERE diag_type_id = v_diag_type_id AND
            diag_subtype_id = v_diag_subtype_id);

    RETURN v_exists;
END
$$;


--
-- Name: generate_partition_triggers_ddl(text, text, text, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION generate_partition_triggers_ddl(table_name text, time_stamp_column_name text, partition_interval text, number_of_interval_of_data_to_keep integer DEFAULT 180) RETURNS text
    LANGUAGE plpgsql IMMUTABLE STRICT
    AS $_$
DECLARE
    ddl_script TEXT;
    timestamp_mask_by_interval TEXT;
    timestamp_mask_length INTEGER;
    insert_stmt TEXT;
    insert_stmt_values TEXT;
    insert_stmt_values_position INTEGER;
    table_name_from_arg ALIAS FOR table_name;
    insert_stmt_position INTEGER;
    insert_stmt_column_name TEXT;
    cascade_delete_stmt TEXT;
BEGIN
    -- Convert the interval to pattern to be used to name partitions
    IF (partition_interval = 'YEAR') THEN
        timestamp_mask_by_interval := 'YYYY';
        timestamp_mask_length := 4;
    ELSEIF (partition_interval = 'MONTH') THEN
        timestamp_mask_by_interval := 'YYYYMM';
        timestamp_mask_length := 6;
    ELSEIF (partition_interval = 'DAY') THEN
        timestamp_mask_by_interval := 'YYYYMMDD';
        timestamp_mask_length := 8;
    ELSEIF (partition_interval = 'HOUR') THEN
        timestamp_mask_by_interval := 'YYYYMMDDHH24';
        timestamp_mask_length := 10;
    ELSEIF (partition_interval = 'MINUTE') THEN
        timestamp_mask_by_interval := 'YYYYMMDDHH24MI';
        timestamp_mask_length := 12;
    ELSE
        RAISE EXCEPTION 'The specified % is not supported interval.  The supported values are YEAR, MONTH, DAY, HOUR, MINUTE', quote_literal(partition_interval);
    END IF;

    -- Verify timestamp datatype
    PERFORM 1 FROM information_schema.columns WHERE information_schema.columns.table_name = table_name_from_arg AND column_name = time_stamp_column_name AND data_type LIKE 'timestamp%';
    IF (NOT FOUND) THEN
        RAISE EXCEPTION 'The specified column % in table % must exists and of type timestamp', quote_literal(time_stamp_column_name), quote_literal(table_name);
    END IF;

    -- Generate insert statement
    insert_stmt := 'EXECUTE(''INSERT INTO '' || quote_ident(''' || table_name || '_'' || to_char(NEW.' || time_stamp_column_name || ', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') || ''
                         (';

    -- Building up the list of column names in the <table_name> (....) section of the INSERT statement
    --     Handling the first column name in the list without the leading commas
    SELECT column_name
        FROM information_schema.columns
        WHERE information_schema.columns.table_name = table_name_from_arg AND ordinal_position = 1
        ORDER BY ordinal_position
        INTO insert_stmt_column_name;
    insert_stmt := insert_stmt || insert_stmt_column_name;
    insert_stmt_values_position := 1;
    insert_stmt_values := 'VALUES
                         ($1';
    --     Handling the rest of the column names with commas separated
    FOR insert_stmt_column_name IN SELECT column_name
        FROM information_schema.columns
        WHERE information_schema.columns.table_name = table_name_from_arg AND ordinal_position > 1
        ORDER BY ordinal_position
    LOOP
        insert_stmt := insert_stmt || ', ' || insert_stmt_column_name;
        insert_stmt_values_position := insert_stmt_values_position + 1;
        insert_stmt_values := insert_stmt_values || ', $' || insert_stmt_values_position;
    END LOOP;
    insert_stmt := insert_stmt || ')
                     ' || insert_stmt_values || ');'')
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

    ddl_script := '-- Script to enable partition for ' || quote_literal(table_name) ||
    ' based on the timestamp value of column ' || quote_literal(time_stamp_column_name) ||
    ' using partition size for every one ' || quote_literal(partition_interval) || '.
    ';
    IF (number_of_interval_of_data_to_keep > 0) THEN
        ddl_script := ddl_script || '-- Drop old partition is enabled because the number_of_interval_of_data_to_keep is greater than 0.';
    ELSE
        ddl_script := ddl_script || '-- Drop old partition is disabled because the number_of_interval_of_data_to_keep is not greater than 0.';
    END IF;

    ddl_script := ddl_script || '
    -- New partition name pattern:  ' || quote_literal(table_name || '_' || timestamp_mask_by_interval || '_' || time_stamp_column_name) || '.' ||
    '
    -------------------------------
    CREATE OR REPLACE FUNCTION ' || table_name || '_partition_handler()
      RETURNS trigger
      LANGUAGE plpgsql VOLATILE
    AS $PARTITION_BODY$
    DECLARE
        table_partition record;
        empty_partition int;
    BEGIN';
        PERFORM 1
            FROM information_schema.columns
            WHERE information_schema.columns.table_name = table_name_from_arg
                AND column_name = time_stamp_column_name
                AND data_type LIKE 'timestamp%'
                AND is_nullable = 'YES';
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
            --
            -- <Place holder for data archive statement to be execute before DROP partition>
            --';
    IF (number_of_interval_of_data_to_keep > 0) THEN
        ddl_script := ddl_script || '
            -- Drop the most recent expired partition
            EXECUTE(''DROP TABLE IF EXISTS '' || quote_ident(''' || table_name || '_'' || to_char(NEW.' || time_stamp_column_name || ' - INTERVAL ''' || number_of_interval_of_data_to_keep || ' ' || partition_interval || ''', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') || '' CASCADE;'');

            -- Clean up any left over partitions that were missed by the DROP statement above
            ---- Step 1 of 2:  Delete left over tuples that are older than the number of tuples to keep';
        FOR cascade_delete_stmt IN
            SELECT '''DELETE FROM ' || kcu.table_name || ' WHERE ' || kcu.column_name || ' IN (SELECT ' || ccu.column_name || ' FROM ' || table_name_from_arg || ' WHERE ' || table_name_from_arg || '.' || time_stamp_column_name || ' < CURRENT_TIMESTAMP - INTERVAL ''''' || (number_of_interval_of_data_to_keep + 1) || ' ' || partition_interval || ''''');'''
            FROM information_schema.table_constraints as tc
                INNER JOIN information_schema.key_column_usage AS kcu
                    ON tc.constraint_name = kcu.constraint_name
                INNER JOIN information_schema.constraint_column_usage AS ccu
                    ON tc.constraint_name = ccu.constraint_name
            WHERE
                tc.constraint_type = 'FOREIGN KEY'
                AND ccu.table_name = table_name_from_arg
        LOOP
            ddl_script := ddl_script || '
            EXECUTE(' || cascade_delete_stmt || ');';
        END LOOP;
        ddl_script := ddl_script || '
            EXECUTE(''DELETE FROM ' || table_name || ' WHERE ' || table_name || '.' || time_stamp_column_name || ' < CURRENT_TIMESTAMP - INTERVAL ''''' || (number_of_interval_of_data_to_keep + 1) || ' ' || partition_interval || ''''';'');

            ---- Step 2 of 2:  Delete empty partitions
            FOR table_partition IN SELECT
                    partition_name
                FROM (
                    SELECT
                        child_table.relname AS partition_name,
                        (regexp_matches(child_table.relname, ''' || table_name || ''' || ''_([0-9]+)_'' || ''' || time_stamp_column_name || '''))[1] AS table_sdate
                    FROM pg_class AS parent_table
                        INNER JOIN pg_inherits AS inheritance
                            ON parent_table.relname=''' || table_name || ''' AND inheritance.inhparent=parent_table.oid
                        INNER JOIN pg_class AS child_table
                            ON inheritance.inhrelid=child_table.oid
                    ) AS TB1
                WHERE CHAR_LENGTH(TB1.table_sdate) = ' || timestamp_mask_length || '
                    AND TO_TIMESTAMP(TB1.table_sdate, ''' || timestamp_mask_by_interval || ''') < CURRENT_TIMESTAMP - INTERVAL ''' || (number_of_interval_of_data_to_keep + 1) || ' ' || partition_interval || '''
            LOOP
                EXECUTE(''SELECT CASE WHEN EXISTS (SELECT 1 FROM '' || QUOTE_IDENT(table_partition.partition_name) || '' LIMIT 1) THEN 0 ELSE 1 END;'') INTO empty_partition;
                IF empty_partition THEN
                    EXECUTE(''DROP TABLE IF EXISTS '' || QUOTE_IDENT(table_partition.partition_name) || '' CASCADE;'');
                END IF;
            END LOOP;
    ';
    END IF;

    ddl_script := ddl_script || '

            -- Create the partition
            BEGIN
                EXECUTE(''CREATE TABLE IF NOT EXISTS '' || QUOTE_IDENT(''' || table_name || '_'' || TO_CHAR(NEW.' || time_stamp_column_name || ', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') ||
                        '' (CHECK ( ' || time_stamp_column_name || ' >= DATE_TRUNC(''''MINUTE'''', TIMESTAMP '''''' || NEW.' || time_stamp_column_name || ' || '''''') '' ||
                        ''      AND ' || time_stamp_column_name || ' <  DATE_TRUNC(''''MINUTE'''', TIMESTAMP '''''' || NEW.' || time_stamp_column_name || ' || '''''' + INTERVAL ''''1 ' || partition_interval || ''''') ) ) '' ||
                        '' INHERITS (' || table_name || '); '');
                EXECUTE(''CREATE INDEX '' || QUOTE_IDENT(''index_' || table_name || '_'' || TO_CHAR(NEW.' || time_stamp_column_name || ', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') ||
                        '' ON '' || QUOTE_IDENT(''' || table_name || '_'' || TO_CHAR(NEW.' || time_stamp_column_name || ', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') ||
                        '' (' || time_stamp_column_name || '); '');
            EXCEPTION
                WHEN duplicate_table THEN
                    -- This error also applies to duplicate index
                    -- Ignore error
            END;
        END IF;
    ';

    ddl_script := ddl_script || '

        ' || insert_stmt || '
        RETURN NULL;
    END;
    $PARTITION_BODY$;
    -------------------------------


    -------------------------------
    ';
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
    v_data_item_id INTEGER := 0;
BEGIN
    -- Get the data item ID
    SELECT data_item_id INTO v_data_item_id
    FROM data_item
    WHERE name = p_data_item;

    -- Does it exist?
    IF (v_data_item_id is null) THEN
        v_data_item_id := 0;
    END IF;

    RETURN v_data_item_id;
END
$$;


--
-- Name: get_diag_subtype_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_diag_subtype_id(p_diag_subtype character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_diag_subtype_id INTEGER := 0;
BEGIN
    -- Get the diagnostic subtype
    SELECT diag_subtype_id INTO v_diag_subtype_id
    FROM diag_subtype
    WHERE name = p_diag_subtype;

    -- Does it exist?
    IF (v_diag_subtype_id is null) THEN
        v_diag_subtype_id := 0;
    END IF;

    RETURN v_diag_subtype_id;
END
$$;


--
-- Name: get_diag_type_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_diag_type_id(p_diag_type character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_diag_type_id INTEGER := 0;
BEGIN
    -- Get the diagnostic type
    SELECT diag_type_id INTO v_diag_type_id
    FROM diag_type
    WHERE name = p_diag_type;

    -- Does it exist?
    IF (v_diag_type_id is null) THEN
        v_diag_type_id := 0;
    END IF;

    RETURN v_diag_type_id;
END
$$;


--
-- Name: get_event_msg(integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_event_msg(key integer) RETURNS character varying
    LANGUAGE plpgsql
    AS $$
DECLARE
    message VARCHAR;
BEGIN
    SELECT value_str INTO message
    FROM event_data
    WHERE event_id = key AND event_data_key_id = 4;
    RETURN message;
END; $$;


--
-- Name: get_feature_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_feature_id(p_feature character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_feature_id INTEGER := 0;
BEGIN
    -- Get the feature ID
    SELECT feature_id INTO v_feature_id
    FROM feature
    WHERE name = p_feature;

    -- Does it exist?
    IF (v_feature_id is null) THEN
        v_feature_id := 0;
    END IF;

    RETURN v_feature_id;
END
$$;


--
-- Name: get_node_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_node_id(p_hostname character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id INTEGER := 0;
BEGIN
    -- Get the node ID
    SELECT node_id INTO v_node_id
    FROM node
    WHERE hostname = p_hostname;

    -- Does it exist?
    IF (v_node_id is null) THEN
        v_node_id := 0;
    END IF;

    RETURN v_node_id;
END
$$;


--
-- Name: get_test_param_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_test_param_id(p_name character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_test_param_id INTEGER := 0;
BEGIN
    -- Get the test parameter ID
    SELECT test_param_id INTO v_test_param_id
    FROM test_param
    WHERE name = p_name;

    -- Does it exist?
    IF (v_test_param_id is null) THEN
        v_test_param_id := 0;
    END IF;

    RETURN v_test_param_id;
END
$$;


--
-- Name: get_test_result_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_test_result_id(p_test_result character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_test_result_id INTEGER := 0;
BEGIN
    -- Get the test result ID
    SELECT test_result_id INTO v_test_result_id
    FROM test_result
    WHERE name = p_test_result;

    -- Does it exist?
    IF (v_test_result_id is null) THEN
        v_test_result_id := 0;
    END IF;

    RETURN v_test_result_id;
END
$$;


--
-- Name: is_numeric(text); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION is_numeric(text) RETURNS boolean
    LANGUAGE plpgsql IMMUTABLE STRICT
    AS $_$
DECLARE
    num NUMERIC;
BEGIN
    num := $1::NUMERIC;
    RETURN TRUE;
    EXCEPTION WHEN OTHERS THEN
        RETURN FALSE;
END
$_$;


--
-- Name: node_feature_exists(integer, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION node_feature_exists(p_node_id integer, p_feature_id integer) RETURNS boolean
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_exists BOOLEAN := FALSE;
BEGIN

    v_exists := exists(
        SELECT 1
        FROM node_feature
        WHERE node_id = p_node_id AND feature_id = p_feature_id);

    RETURN v_exists;
END
$$;


--
-- Name: orcm_comma_list_to_regex(text, boolean); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION orcm_comma_list_to_regex(list text, comma boolean DEFAULT true) RETURNS text
    LANGUAGE plpgsql
    AS $_$
BEGIN
    IF( list IS NULL ) THEN
        list := '.*';
    ELSE
        list := REGEXP_REPLACE(list, E'([[\\](){}.+^$|\\\\?-])', E'\\\\\\1', 'g');
        list := REGEXP_REPLACE(list, E'\\*', '.*','g');
        IF( comma IS TRUE ) THEN
            list := '(^' || REGEXP_REPLACE(list, ',', '$)|(^', 'g') || '$)';
        ELSE
            list := '^' || list || '$';
        END IF;
    END IF;

    RETURN list;
END
$_$;


--
-- Name: purge_data(text, text, integer, text); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION purge_data(name_of_table text, name_of_timestamp_column text, number_of_time_unit_from_now integer, time_unit text) RETURNS void
    LANGUAGE plpgsql
    AS $$
        BEGIN
            EXECUTE('DELETE FROM ' || name_of_table || ' WHERE ' || name_of_table || '.' || name_of_timestamp_column || ' < CURRENT_TIMESTAMP - interval ''' || number_of_time_unit_from_now || ' ' || time_unit || ''';');
        END
        $$;


--
-- Name: query_event_data(timestamp without time zone, timestamp without time zone, text, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION query_event_data(ts_from timestamp without time zone, ts_to timestamp without time zone, nodes text, num_limit integer) RETURNS TABLE("EVENT ID" text, "TIME" text, "SEVERITY" text, "TYPE" text, "HOSTNAME" text, "MESSAGE" text)
    LANGUAGE plpgsql
    AS $$
DECLARE
    ts_aux TIMESTAMP;
BEGIN
    IF( ts_from > ts_to) THEN
        ts_aux := ts_from;
        ts_from := ts_to;
        ts_to := ts_aux;
    ELSIF( NOT ISFINITE(ts_from) ) THEN
        ts_from := '-INFINITY'::TIMESTAMP;
    END IF;

    nodes := orcm_comma_list_to_regex(nodes);

    RETURN QUERY
    SELECT e.event_id::TEXT, e.time_stamp::TEXT, e.severity::TEXT, e.type::TEXT,
        ed.value_str::TEXT, ed2.value_str::TEXT
    FROM event e INNER JOIN event_data ed ON e.event_id = ed.event_id
            AND ed.event_data_key_id = 1
        INNER JOIN event_data ed2 ON e.event_id = ed2.event_id
            AND ed2.event_data_key_id = 4
    WHERE e.time_stamp BETWEEN ts_from AND ts_to
        AND ed.value_str ~* nodes
    LIMIT num_limit;
END
$$;


--
-- Name: query_event_snsr_data(integer, interval, text, text, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION query_event_snsr_data(id_event integer, search_interval interval, sensors text, nodes text, num_limit integer) RETURNS TABLE("TIME" text, "HOSTNAME" text, "DATA ITEM" text, "VALUE" text, "UNITS" text)
    LANGUAGE plpgsql
    AS $$
DECLARE
    ts_from TIMESTAMP;
    ts_to TIMESTAMP;
BEGIN
    SELECT e.time_stamp INTO ts_from FROM event e WHERE e.event_id = id_event;

    IF( search_interval IS NULL ) THEN
        ts_to := ts_from;
        ts_from := '-INFINITY'::TIMESTAMP;
    ELSIF( '00:00:00'::INTERVAL < search_interval ) THEN
        ts_to := ts_from + search_interval;
    ELSE
        ts_to := ts_from;
        ts_from := ts_to + search_interval;
    END IF;

    sensors := orcm_comma_list_to_regex(sensors);
    nodes := orcm_comma_list_to_regex(nodes);

    RETURN QUERY
    SELECT d.time_stamp::TEXT, d.hostname::TEXT, d.data_item::TEXT,
        COALESCE(d.value_real::TEXT, d.value_int::TEXT, d.value_str::TEXT), d.units::TEXT
    FROM data_sample_raw d
    WHERE d.time_stamp BETWEEN ts_from AND ts_to
        AND d.data_item ~* sensors AND d.hostname ~* nodes
    LIMIT num_limit;
END
$$;


--
-- Name: query_idle(interval, text, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION query_idle(idle_interval interval, nodes text, num_limit integer) RETURNS TABLE("HOSTNAME" text, "IDLE TIME" text)
    LANGUAGE plpgsql
    AS $$
BEGIN
    nodes := orcm_comma_list_to_regex(nodes);

    RETURN QUERY
    SELECT d.hostname::TEXT,
        (NOW() - MAX(d.time_stamp))::INTERVAL::TEXT
    FROM data_sample_raw d
    GROUP BY d.hostname
    HAVING d.hostname ~* nodes AND
        (CASE WHEN( idle_interval IS NOT NULL ) THEN
                CASE WHEN((NOW() - MAX(d.time_stamp))::INTERVAL >= idle_interval) THEN TRUE ELSE FALSE END
            ELSE TRUE
        END)
    LIMIT num_limit;
END
$$;


--
-- Name: query_log(text, timestamp without time zone, timestamp without time zone, text, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION query_log(val text, ts_from timestamp without time zone, ts_to timestamp without time zone, nodes text, num_limit integer) RETURNS TABLE("HOSTNAME" text, "DATA ITEM" text, "TIME" text, "MESSAGE" text)
    LANGUAGE plpgsql
    AS $$
DECLARE
    ts_aux TIMESTAMP;
BEGIN
    IF( ts_from > ts_to) THEN
        ts_aux := ts_from;
        ts_from := ts_to;
        ts_to := ts_aux;
    ELSIF( NOT ISFINITE(ts_from) ) THEN
        ts_from := '-INFINITY'::TIMESTAMP;
    END IF;

    nodes := orcm_comma_list_to_regex(nodes);
    val := orcm_comma_list_to_regex(val, FALSE);

    RETURN QUERY
    SELECT d.hostname::TEXT, d.data_item::TEXT, d.time_stamp::TEXT, d.value_str::TEXT
    FROM data_sample_raw d
    WHERE d.hostname ~* nodes AND d.data_item::TEXT = 'syslog_log_message'::TEXT
        AND d.value_str ~* val AND d.time_stamp BETWEEN ts_from AND ts_to
    LIMIT num_limit;
END
$$;


--
-- Name: query_node_status(text, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION query_node_status(nodes text, num_limit integer) RETURNS TABLE("HOSTNAME" text, "STATUS" text)
    LANGUAGE plpgsql
    AS $$
BEGIN
    nodes := orcm_comma_list_to_regex(nodes);

    RETURN QUERY
    SELECT n.hostname::TEXT, n.status::TEXT
    FROM node n
    WHERE n.hostname ~* nodes
    LIMIT num_limit;
END
$$;


--
-- Name: query_sensor_history(text, timestamp without time zone, timestamp without time zone, text, text, text, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION query_sensor_history(sensors text, ts_from timestamp without time zone, ts_to timestamp without time zone, val_from text, val_to text, nodes text, num_limit integer) RETURNS TABLE("TIME" text, "HOSTNAME" text, "DATA ITEM" text, "VALUE" text, "UNITS" text)
    LANGUAGE plpgsql
    AS $$
DECLARE
    ts_aux TIMESTAMP;
    val_daux DOUBLE PRECISION;
    val_dfrom DOUBLE PRECISION;
    val_dto DOUBLE PRECISION;
BEGIN
    IF( ts_from > ts_to) THEN
        ts_aux := ts_from;
        ts_from := ts_to;
        ts_to := ts_aux;
    ELSIF( NOT ISFINITE(ts_from) ) THEN
        ts_from := '-INFINITY'::TIMESTAMP;
    END IF;

    IF( val_from IS NULL OR val_from = '*' ) THEN
        val_dfrom := '-INFINITY'::DOUBLE PRECISION;
        val_from := orcm_comma_list_to_regex(val_from);
    ELSIF( is_numeric(val_from) ) THEN
        val_dfrom := val_from::DOUBLE PRECISION;
        val_from := NULL;
    ELSE
        val_from := orcm_comma_list_to_regex(val_from);
    END IF;

    IF( val_dfrom IS NOT NULL ) THEN
        IF( is_numeric(val_to) ) THEN
            val_dto := val_to::DOUBLE PRECISION;
        ELSE
            val_dto := 'INFINITY'::DOUBLE PRECISION;
        END IF;
    END IF;

    IF( val_dfrom > val_dto ) THEN
        val_daux := val_dfrom;
        val_dfrom := val_dto;
        val_dto := val_daux;
    END IF;

    nodes := orcm_comma_list_to_regex(nodes);
    sensors := orcm_comma_list_to_regex(sensors);

    RETURN QUERY
    SELECT d.time_stamp::TEXT, d.hostname::TEXT, d.data_item::TEXT,
        COALESCE(d.value_real::TEXT, d.value_int::TEXT, d.value_str::TEXT),
        d.units::TEXT
    FROM data_sample_raw d
    WHERE d.time_stamp BETWEEN ts_from AND ts_to AND d.data_item ~* sensors
        AND d.hostname ~* nodes AND
        (CASE WHEN( d.value_real IS NOT NULL ) THEN
                CASE WHEN (d.value_real BETWEEN val_dfrom AND val_dto) THEN TRUE ELSE FALSE END
            WHEN( d.value_int IS NOT NULL ) THEN
                CASE WHEN (d.value_int BETWEEN val_dfrom AND val_dto) THEN TRUE ELSE FALSE END
            WHEN( d.value_str IS NOT NULL ) THEN
                CASE WHEN (d.value_str ~* val_from) THEN TRUE ELSE FALSE END
            ELSE TRUE
        END)
    LIMIT num_limit;
END
$$;


--
-- Name: query_snsr_get_inventory(text, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION query_snsr_get_inventory(nodes text, num_limit integer) RETURNS TABLE("HOSTNAME" text, "PLUGIN NAME" text, "SENSOR NAME" text)
    LANGUAGE plpgsql
    AS $_$
BEGIN
    nodes := orcm_comma_list_to_regex(nodes);

    RETURN QUERY
    SELECT n.hostname::TEXT, REGEXP_REPLACE(f.name::TEXT, '(^sensor_)|(_\d+$)','','gi'),
        COALESCE(nf.value_int::TEXT, nf.value_real::TEXT, nf.value_str::TEXT)
    FROM node_feature nf INNER JOIN node n ON n.hostname ~* nodes
        AND n.node_id = nf.node_id
    INNER JOIN feature f ON f.name ~~* 'sensor%' AND f.feature_id = nf.feature_id
    LIMIT num_limit;
END
$_$;


--
-- Name: record_data_sample(character varying, character varying, character varying, timestamp without time zone, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION record_data_sample(p_hostname character varying, p_data_group character varying, p_data_item character varying, p_time_stamp timestamp without time zone, p_data_type_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id INTEGER := 0;
    v_data_item_id INTEGER := 0;
    v_data_item_name VARCHAR(250);
BEGIN
    -- Compose the data item name from the data group and item itself
    v_data_item_name := p_data_group || '_';
    v_data_item_name := v_data_item_name || p_data_item;

    -- Insert in the data_sample_raw table
    INSERT INTO data_sample_raw(
        hostname,
        data_item,
        time_stamp,
        value_int,
        value_real,
        value_str,
        units,
        data_type_id)
    VALUES(
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
    --IF (v_node_id = 0) THEN
    --    v_node_id := add_node(p_hostname);
    --END IF;

    --IF (not data_type_exists(p_data_type_id)) THEN
    --    PERFORM add_data_type(p_data_type_id, NULL);
    --END IF;

    -- Get the data item ID
    --v_data_item_id := get_data_item_id(v_data_item_name);
    --IF (v_data_item_id = 0) THEN
    --    v_data_item_id := add_data_item(v_data_item_name, p_data_type_id);
    --END IF;

    -- Add the data sample
    --PERFORM add_data_sample(
    --    v_node_id,
    --    v_data_item_id,
    --    p_time_stamp,
    --    p_value_int,
    --    p_value_real,
    --    p_value_str,
    --    p_units);

    RETURN;
END;
$$;


--
-- Name: record_diag_test_config(character varying, character varying, character varying, timestamp without time zone, character varying, integer, bigint, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION record_diag_test_config(p_hostname character varying, p_diag_type character varying, p_diag_subtype character varying, p_start_time timestamp without time zone, p_test_param character varying, p_data_type_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id INTEGER := 0;
    v_diag_type_id INTEGER := 0;
    v_diag_subtype_id INTEGER := 0;
    v_test_param_id INTEGER := 0;
BEGIN
    --Get the node ID
    v_node_id := get_node_id(p_hostname);
    IF (v_node_id = 0) THEN
        v_node_id := add_node(p_hostname);
    END IF;

    -- Get the diagnostic type ID
    v_diag_type_id := get_diag_type_id(p_diag_type);
    IF (v_diag_type_id = 0) THEN
        v_diag_type_id := add_diag_type(p_diag_type);
    END IF;

    -- Get the diagnostic subtype ID
    v_diag_subtype_id := get_diag_subtype_id(p_diag_subtype);
    IF (v_diag_subtype_id = 0) THEN
        v_diag_subtype_id := add_diag_subtype(p_diag_subtype);
    END IF;

    IF (not data_type_exists(p_data_type_id)) THEN
        PERFORM add_data_type(p_data_type_id, NULL);
    END IF;

    -- Get the test parameter ID
    v_test_param_id := get_test_param_id(p_test_param);
    IF (v_test_param_id = 0) THEN
        v_test_param_id := add_test_param(p_test_param, p_data_type_id);
    END IF;

    -- Add the test configuration parameter
    PERFORM add_diag_test_config(
        v_node_id,
        v_diag_type_id,
        v_diag_subtype_id,
        p_start_time,
        v_test_param_id,
        p_value_int,
        p_value_real,
        p_value_str,
        p_units);

    RETURN;
END
$$;


--
-- Name: record_diag_test_result(character varying, character varying, character varying, timestamp without time zone, timestamp without time zone, integer, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION record_diag_test_result(p_hostname character varying, p_diag_type character varying, p_diag_subtype character varying, p_start_time timestamp without time zone, p_end_time timestamp without time zone, p_component_index integer, p_test_result character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id INTEGER := 0;
    v_diag_type_id INTEGER := 0;
    v_diag_subtype_id INTEGER := 0;
    v_test_result_id INTEGER := 0;
BEGIN
    -- Get the node ID
    v_node_id := get_node_id(p_hostname);
    IF (v_node_id = 0) THEN
        v_node_id := add_node(p_hostname);
    END IF;

    -- Get the diagnostic type ID
    v_diag_type_id := get_diag_type_id(p_diag_type);
    IF (v_diag_type_id = 0) THEN
        v_diag_type_id := add_diag_type(p_diag_type);
    END IF;

    -- Get the diagnostic subtype ID
    v_diag_subtype_id := get_diag_subtype_id(p_diag_subtype);
    IF (v_diag_subtype_id = 0) THEN
        v_diag_subtype_id := add_diag_subtype(p_diag_subtype);
    END IF;

    -- If this particular diagnostic does not exist, add it
    IF (not diag_exists(v_diag_type_id, v_diag_subtype_id)) THEN
        PERFORM add_diag(v_diag_type_id, v_diag_subtype_id);
    END IF;

    -- Get the test result ID
    v_test_result_id := get_test_result_id(p_test_result);
    IF (v_test_result_id = 0) THEN
        v_test_result_id := add_test_result(p_test_result);
    END IF;

    -- Add the diagnostic test result
    PERFORM add_diag_test(
        v_node_id,
        v_diag_type_id,
        v_diag_subtype_id,
        p_start_time,
        p_end_time,
        p_component_index,
        v_test_result_id);

    RETURN;
END
$$;


--
-- Name: set_node_feature(character varying, character varying, integer, bigint, double precision, character varying, character varying, timestamp without time zone); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION set_node_feature(p_hostname character varying, p_feature character varying, p_data_type_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying, p_time_stamp timestamp without time zone) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id INTEGER := 0;
    v_feature_id INTEGER := 0;
    v_node_feature_exists BOOLEAN := TRUE;
BEGIN
    IF (p_time_stamp IS NULL) THEN
        SELECT CURRENT_TIMESTAMP INTO p_time_stamp;
    END IF;
    -- Get the node ID
    v_node_id := get_node_id(p_hostname);
    IF (v_node_id = 0) THEN
        v_node_id := add_node(p_hostname);
        v_node_feature_exists := FALSE;
    END IF;

    IF (not data_type_exists(p_data_type_id)) THEN
        PERFORM add_data_type(p_data_type_id, NULL);
    END IF;

    -- Get the feature ID
    v_feature_id := get_feature_id(p_feature);
    IF (v_feature_id = 0) THEN
        v_feature_id := add_feature(p_feature, p_data_type_id);
        v_node_feature_exists := FALSE;
    END IF;

    -- Has this feature already been added for this node?
    IF (v_node_feature_exists) THEN
        v_node_feature_exists := node_feature_exists(v_node_id, v_feature_id);
    END IF;

    IF (v_node_feature_exists) THEN
        PERFORM update_node_feature(
            v_node_id,
            v_feature_id,
            p_value_int,
            p_value_real,
            p_value_str,
            p_units,
            p_time_stamp);
    ELSE
        PERFORM add_node_feature(
            v_node_id,
            v_feature_id,
            p_value_int,
            p_value_real,
            p_value_str,
            p_units,
            p_time_stamp);
    END IF;

    RETURN;
END
$$;


--
-- Name: update_node_feature(integer, integer, bigint, double precision, character varying, character varying, timestamp without time zone); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION update_node_feature(p_node_id integer, p_feature_id integer, p_value_int bigint, p_value_real double precision, p_value_str character varying, p_units character varying, p_time_stamp timestamp without time zone) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    IF (p_time_stamp IS NULL) THEN
        SELECT CURRENT_TIMESTAMP INTO p_time_stamp;
    END IF;
    UPDATE node_feature
    SET value_int = p_value_int,
        value_real = p_value_real,
        value_str = p_value_str,
        units = p_units,
        time_stamp = p_time_stamp
    WHERE node_id = p_node_id AND feature_id = p_feature_id;
END
$$;


SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: alembic_version; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE alembic_version (
    version_num character varying(32) NOT NULL
);


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
-- Name: data_sample_raw; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE data_sample_raw (
    hostname character varying(256) NOT NULL,
    data_item character varying(250) NOT NULL,
    time_stamp timestamp without time zone NOT NULL,
    value_int bigint,
    value_real double precision,
    value_str text,
    units character varying(50),
    data_type_id integer NOT NULL,
    app_value_type_id integer,
    event_id integer,
    data_sample_id integer NOT NULL,
    CONSTRAINT ck_data_sample_raw_at_least_one_value CHECK ((((value_int IS NOT NULL) OR (value_real IS NOT NULL)) OR (value_str IS NOT NULL)))
);


--
-- Name: data_sample_raw_data_sample_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE data_sample_raw_data_sample_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: data_sample_raw_data_sample_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE data_sample_raw_data_sample_id_seq OWNED BY data_sample_raw.data_sample_id;


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
-- Name: data_sensors_view; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW data_sensors_view AS
 SELECT dsr.hostname,
    dsr.data_item,
    concat(dsr.time_stamp) AS time_stamp,
    concat(dsr.value_int, dsr.value_real, dsr.value_str) AS value_str,
    dsr.units
   FROM data_sample_raw dsr;


--
-- Name: data_type; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE data_type (
    data_type_id integer NOT NULL,
    name character varying(150)
);


--
-- Name: data_type_data_type_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE data_type_data_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: data_type_data_type_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE data_type_data_type_id_seq OWNED BY data_type.data_type_id;


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
    value_str text,
    units character varying(50),
    value_int bigint,
    CONSTRAINT ck_diag_test_config_at_least_one_value CHECK ((((value_int IS NOT NULL) OR (value_real IS NOT NULL)) OR (value_str IS NOT NULL)))
);


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
    event_id integer NOT NULL,
    time_stamp timestamp without time zone DEFAULT now() NOT NULL,
    severity character varying(50) DEFAULT ''::character varying NOT NULL,
    type character varying(50) DEFAULT ''::character varying NOT NULL,
    vendor character varying(250) DEFAULT ''::character varying NOT NULL,
    version character varying(250) DEFAULT ''::character varying NOT NULL,
    description text DEFAULT ''::text NOT NULL
);


--
-- Name: event_data; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE event_data (
    event_data_id integer NOT NULL,
    event_id integer NOT NULL,
    event_data_key_id integer NOT NULL,
    value_int bigint,
    value_real double precision,
    value_str text,
    units character varying(50) DEFAULT ''::character varying NOT NULL,
    CONSTRAINT ck_event_data_at_least_one_value CHECK ((((value_int IS NOT NULL) OR (value_real IS NOT NULL)) OR (value_str IS NOT NULL)))
);


--
-- Name: event_data_event_data_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE event_data_event_data_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: event_data_event_data_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE event_data_event_data_id_seq OWNED BY event_data.event_data_id;


--
-- Name: event_data_key; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE event_data_key (
    event_data_key_id integer NOT NULL,
    name character varying(250) NOT NULL,
    app_value_type_id integer NOT NULL
);


--
-- Name: event_data_key_event_data_key_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE event_data_key_event_data_key_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: event_data_key_event_data_key_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE event_data_key_event_data_key_id_seq OWNED BY event_data_key.event_data_key_id;


--
-- Name: event_event_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE event_event_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: event_event_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE event_event_id_seq OWNED BY event.event_id;


--
-- Name: event_sensor_data_view; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW event_sensor_data_view AS
 SELECT (data_sample_raw.time_stamp)::text AS time_stamp,
    data_sample_raw.hostname,
    data_sample_raw.data_item,
    concat((data_sample_raw.value_int)::text, (data_sample_raw.value_real)::text, data_sample_raw.value_str) AS value,
    data_sample_raw.units
   FROM data_sample_raw;


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
    value_str text,
    units character varying(50),
    value_int bigint,
    time_stamp timestamp without time zone DEFAULT now() NOT NULL
);


--
-- Name: node_features_view; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW node_features_view AS
 SELECT (node_feature.node_id)::text AS node_id,
    node.hostname,
    (node_feature.feature_id)::text AS feature_id,
    feature.name AS feature,
    COALESCE(''::text) AS value_int,
    COALESCE(''::text) AS value_real,
    concat((node_feature.value_int)::text, (node_feature.value_real)::text, node_feature.value_str) AS value_str,
    node_feature.units,
    (node_feature.time_stamp)::text AS time_stamp
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
-- Name: syslog_view; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW syslog_view AS
 SELECT data_sample_raw.hostname,
    data_sample_raw.data_item,
    (data_sample_raw.time_stamp)::text AS time_stamp,
    data_sample_raw.value_str AS log
   FROM data_sample_raw
  WHERE ((data_sample_raw.data_item)::text ~~ 'syslog_log%'::text);


--
-- Name: test_param; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE test_param (
    test_param_id integer NOT NULL,
    name character varying(250) NOT NULL,
    data_type_id integer NOT NULL
);


--
-- Name: test_param_test_param_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE test_param_test_param_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: test_param_test_param_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE test_param_test_param_id_seq OWNED BY test_param.test_param_id;


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
-- Name: data_sample_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_sample_raw ALTER COLUMN data_sample_id SET DEFAULT nextval('data_sample_raw_data_sample_id_seq'::regclass);


--
-- Name: data_type_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_type ALTER COLUMN data_type_id SET DEFAULT nextval('data_type_data_type_id_seq'::regclass);


--
-- Name: diag_subtype_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_subtype ALTER COLUMN diag_subtype_id SET DEFAULT nextval('diag_subtype_diag_subtype_id_seq'::regclass);


--
-- Name: diag_type_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_type ALTER COLUMN diag_type_id SET DEFAULT nextval('diag_type_diag_type_id_seq'::regclass);


--
-- Name: event_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY event ALTER COLUMN event_id SET DEFAULT nextval('event_event_id_seq'::regclass);


--
-- Name: event_data_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY event_data ALTER COLUMN event_data_id SET DEFAULT nextval('event_data_event_data_id_seq'::regclass);


--
-- Name: event_data_key_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY event_data_key ALTER COLUMN event_data_key_id SET DEFAULT nextval('event_data_key_event_data_key_id_seq'::regclass);


--
-- Name: feature_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY feature ALTER COLUMN feature_id SET DEFAULT nextval('feature_feature_id_seq'::regclass);


--
-- Name: node_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY node ALTER COLUMN node_id SET DEFAULT nextval('node_node_id_seq'::regclass);


--
-- Name: test_param_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY test_param ALTER COLUMN test_param_id SET DEFAULT nextval('test_param_test_param_id_seq'::regclass);


--
-- Name: test_result_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY test_result ALTER COLUMN test_result_id SET DEFAULT nextval('test_result_test_result_id_seq'::regclass);


--
-- Data for Name: alembic_version; Type: TABLE DATA; Schema: public; Owner: -
--

COPY alembic_version (version_num) FROM stdin;
0fb66069a81f
\.


--
-- Data for Name: data_item; Type: TABLE DATA; Schema: public; Owner: -
--

COPY data_item (data_item_id, name, data_type_id) FROM stdin;
\.


--
-- Name: data_item_data_item_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('data_item_data_item_id_seq', 1, false);


--
-- Data for Name: data_sample_raw; Type: TABLE DATA; Schema: public; Owner: -
--

COPY data_sample_raw (hostname, data_item, time_stamp, value_int, value_real, value_str, units, data_type_id, app_value_type_id, event_id, data_sample_id) FROM stdin;
\.


--
-- Name: data_sample_raw_data_sample_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('data_sample_raw_data_sample_id_seq', 1, false);


--
-- Data for Name: data_type; Type: TABLE DATA; Schema: public; Owner: -
--

COPY data_type (data_type_id, name) FROM stdin;
\.


--
-- Name: data_type_data_type_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('data_type_data_type_id_seq', 1, false);


--
-- Data for Name: diag; Type: TABLE DATA; Schema: public; Owner: -
--

COPY diag (diag_type_id, diag_subtype_id) FROM stdin;
\.


--
-- Data for Name: diag_subtype; Type: TABLE DATA; Schema: public; Owner: -
--

COPY diag_subtype (diag_subtype_id, name) FROM stdin;
\.


--
-- Name: diag_subtype_diag_subtype_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('diag_subtype_diag_subtype_id_seq', 1, false);


--
-- Data for Name: diag_test; Type: TABLE DATA; Schema: public; Owner: -
--

COPY diag_test (node_id, diag_type_id, diag_subtype_id, start_time, end_time, component_index, test_result_id) FROM stdin;
\.


--
-- Data for Name: diag_test_config; Type: TABLE DATA; Schema: public; Owner: -
--

COPY diag_test_config (node_id, diag_type_id, diag_subtype_id, start_time, test_param_id, value_real, value_str, units, value_int) FROM stdin;
\.


--
-- Data for Name: diag_type; Type: TABLE DATA; Schema: public; Owner: -
--

COPY diag_type (diag_type_id, name) FROM stdin;
\.


--
-- Name: diag_type_diag_type_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('diag_type_diag_type_id_seq', 1, false);


--
-- Data for Name: event; Type: TABLE DATA; Schema: public; Owner: -
--

COPY event (event_id, time_stamp, severity, type, vendor, version, description) FROM stdin;
\.


--
-- Data for Name: event_data; Type: TABLE DATA; Schema: public; Owner: -
--

COPY event_data (event_data_id, event_id, event_data_key_id, value_int, value_real, value_str, units) FROM stdin;
\.


--
-- Name: event_data_event_data_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('event_data_event_data_id_seq', 1, false);


--
-- Data for Name: event_data_key; Type: TABLE DATA; Schema: public; Owner: -
--

COPY event_data_key (event_data_key_id, name, app_value_type_id) FROM stdin;
\.


--
-- Name: event_data_key_event_data_key_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('event_data_key_event_data_key_id_seq', 1, false);


--
-- Name: event_event_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('event_event_id_seq', 1, false);


--
-- Data for Name: feature; Type: TABLE DATA; Schema: public; Owner: -
--

COPY feature (feature_id, name, data_type_id) FROM stdin;
\.


--
-- Name: feature_feature_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('feature_feature_id_seq', 1, false);


--
-- Data for Name: node; Type: TABLE DATA; Schema: public; Owner: -
--

COPY node (node_id, hostname, node_type, status, num_cpus, num_sockets, cores_per_socket, threads_per_core, memory) FROM stdin;
\.


--
-- Data for Name: node_feature; Type: TABLE DATA; Schema: public; Owner: -
--

COPY node_feature (node_id, feature_id, value_real, value_str, units, value_int, time_stamp) FROM stdin;
\.


--
-- Name: node_node_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('node_node_id_seq', 1, false);


--
-- Data for Name: test_param; Type: TABLE DATA; Schema: public; Owner: -
--

COPY test_param (test_param_id, name, data_type_id) FROM stdin;
\.


--
-- Name: test_param_test_param_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('test_param_test_param_id_seq', 1, false);


--
-- Data for Name: test_result; Type: TABLE DATA; Schema: public; Owner: -
--

COPY test_result (test_result_id, name) FROM stdin;
\.


--
-- Name: test_result_test_result_id_seq; Type: SEQUENCE SET; Schema: public; Owner: -
--

SELECT pg_catalog.setval('test_result_test_result_id_seq', 1, false);


--
-- Name: pk_data_item; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_item
    ADD CONSTRAINT pk_data_item PRIMARY KEY (data_item_id);


--
-- Name: pk_data_sample_raw; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_sample_raw
    ADD CONSTRAINT pk_data_sample_raw PRIMARY KEY (data_sample_id);


--
-- Name: pk_data_type; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_type
    ADD CONSTRAINT pk_data_type PRIMARY KEY (data_type_id);


--
-- Name: pk_diag; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag
    ADD CONSTRAINT pk_diag PRIMARY KEY (diag_type_id, diag_subtype_id);


--
-- Name: pk_diag_subtype; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_subtype
    ADD CONSTRAINT pk_diag_subtype PRIMARY KEY (diag_subtype_id);


--
-- Name: pk_diag_test; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_test
    ADD CONSTRAINT pk_diag_test PRIMARY KEY (node_id, diag_type_id, diag_subtype_id, start_time);


--
-- Name: pk_diag_test_config; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_test_config
    ADD CONSTRAINT pk_diag_test_config PRIMARY KEY (node_id, diag_type_id, diag_subtype_id, start_time, test_param_id);


--
-- Name: pk_diag_type; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_type
    ADD CONSTRAINT pk_diag_type PRIMARY KEY (diag_type_id);


--
-- Name: pk_event; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY event
    ADD CONSTRAINT pk_event PRIMARY KEY (event_id);


--
-- Name: pk_event_data; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY event_data
    ADD CONSTRAINT pk_event_data PRIMARY KEY (event_data_id);


--
-- Name: pk_event_data_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY event_data_key
    ADD CONSTRAINT pk_event_data_key PRIMARY KEY (event_data_key_id);


--
-- Name: pk_feature; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY feature
    ADD CONSTRAINT pk_feature PRIMARY KEY (feature_id);


--
-- Name: pk_node; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY node
    ADD CONSTRAINT pk_node PRIMARY KEY (node_id);


--
-- Name: pk_node_feature; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY node_feature
    ADD CONSTRAINT pk_node_feature PRIMARY KEY (node_id, feature_id);


--
-- Name: pk_test_param; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY test_param
    ADD CONSTRAINT pk_test_param PRIMARY KEY (test_param_id);


--
-- Name: pk_test_result; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY test_result
    ADD CONSTRAINT pk_test_result PRIMARY KEY (test_result_id);


--
-- Name: uq_data_item_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_item
    ADD CONSTRAINT uq_data_item_name UNIQUE (name);


--
-- Name: uq_data_type_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_type
    ADD CONSTRAINT uq_data_type_name UNIQUE (name);


--
-- Name: uq_diag_subtype_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_subtype
    ADD CONSTRAINT uq_diag_subtype_name UNIQUE (name);


--
-- Name: uq_diag_type_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY diag_type
    ADD CONSTRAINT uq_diag_type_name UNIQUE (name);


--
-- Name: uq_event_data_key_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY event_data_key
    ADD CONSTRAINT uq_event_data_key_name UNIQUE (name);


--
-- Name: uq_feature_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY feature
    ADD CONSTRAINT uq_feature_name UNIQUE (name);


--
-- Name: uq_node_hostname; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY node
    ADD CONSTRAINT uq_node_hostname UNIQUE (hostname);


--
-- Name: uq_test_param_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY test_param
    ADD CONSTRAINT uq_test_param_name UNIQUE (name);


--
-- Name: uq_test_result_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY test_result
    ADD CONSTRAINT uq_test_result_name UNIQUE (name);


--
-- Name: fk_data_item_data_type_id_data_type; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_item
    ADD CONSTRAINT fk_data_item_data_type_id_data_type FOREIGN KEY (data_type_id) REFERENCES data_type(data_type_id);


--
-- Name: fk_data_sample_raw_event_id_event; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_sample_raw
    ADD CONSTRAINT fk_data_sample_raw_event_id_event FOREIGN KEY (event_id) REFERENCES event(event_id);


--
-- Name: fk_diag_diag_subtype_id_diag_subtype; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag
    ADD CONSTRAINT fk_diag_diag_subtype_id_diag_subtype FOREIGN KEY (diag_subtype_id) REFERENCES diag_subtype(diag_subtype_id);


--
-- Name: fk_diag_diag_type_id_diag_type; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag
    ADD CONSTRAINT fk_diag_diag_type_id_diag_type FOREIGN KEY (diag_type_id) REFERENCES diag_type(diag_type_id);


--
-- Name: fk_diag_test_config_node_id_diag_test; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_test_config
    ADD CONSTRAINT fk_diag_test_config_node_id_diag_test FOREIGN KEY (node_id, diag_type_id, diag_subtype_id, start_time) REFERENCES diag_test(node_id, diag_type_id, diag_subtype_id, start_time);


--
-- Name: fk_diag_test_config_test_param_id_test_param; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_test_config
    ADD CONSTRAINT fk_diag_test_config_test_param_id_test_param FOREIGN KEY (test_param_id) REFERENCES test_param(test_param_id);


--
-- Name: fk_diag_test_diag_type_id_diag; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_test
    ADD CONSTRAINT fk_diag_test_diag_type_id_diag FOREIGN KEY (diag_type_id, diag_subtype_id) REFERENCES diag(diag_type_id, diag_subtype_id);


--
-- Name: fk_diag_test_node_id_node; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY diag_test
    ADD CONSTRAINT fk_diag_test_node_id_node FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: fk_event_data_event_data_key_id_event_data_key; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY event_data
    ADD CONSTRAINT fk_event_data_event_data_key_id_event_data_key FOREIGN KEY (event_data_key_id) REFERENCES event_data_key(event_data_key_id);


--
-- Name: fk_event_data_event_id_event; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY event_data
    ADD CONSTRAINT fk_event_data_event_id_event FOREIGN KEY (event_id) REFERENCES event(event_id);


--
-- Name: fk_feature_data_type_id_data_type; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY feature
    ADD CONSTRAINT fk_feature_data_type_id_data_type FOREIGN KEY (data_type_id) REFERENCES data_type(data_type_id);


--
-- Name: fk_node_feature_feature_id_feature; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY node_feature
    ADD CONSTRAINT fk_node_feature_feature_id_feature FOREIGN KEY (feature_id) REFERENCES feature(feature_id);


--
-- Name: fk_node_feature_node_id_node; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY node_feature
    ADD CONSTRAINT fk_node_feature_node_id_node FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: fk_test_param_data_type_id_data_type; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY test_param
    ADD CONSTRAINT fk_test_param_data_type_id_data_type FOREIGN KEY (data_type_id) REFERENCES data_type(data_type_id);


--
-- Name: public; Type: ACL; Schema: -; Owner: -
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM postgres;
GRANT ALL ON SCHEMA public TO postgres;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- PostgreSQL database dump complete
--

