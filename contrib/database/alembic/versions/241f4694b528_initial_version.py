"""Initial Version

Revision ID: 241f4694b528
Revises: None
Create Date: 2015-09-11 16:47:54.734313

"""

# revision identifiers, used by Alembic.
revision = '241f4694b528'
down_revision = None
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap


def _postgresql_upgrade_ddl():
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW data_samples_view AS
            SELECT data_sample_raw.hostname,
                data_sample_raw.data_item,
                data_sample_raw.data_type_id,
                data_sample_raw.time_stamp,
                data_sample_raw.value_int,
                data_sample_raw.value_real,
                data_sample_raw.value_str,
                data_sample_raw.units
            FROM data_sample_raw;
        """))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW node_features_view AS
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
        """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_data_item(p_name CHARACTER VARYING, p_data_type_id INTEGER) RETURNS INTEGER
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
        $$;"""))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_data_sample(p_node_id INTEGER, p_data_item_id INTEGER, p_time_stamp TIMESTAMP WITHOUT TIME ZONE, p_value_int BIGINT, p_value_real DOUBLE PRECISION, p_value_str CHARACTER VARYING, p_units CHARACTER VARYING) RETURNS VOID
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
        $$;"""))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_data_type(p_data_type_id INTEGER, p_name CHARACTER VARYING) RETURNS VOID
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_diag(p_diag_type_id INTEGER, p_diag_subtype_id INTEGER) RETURNS VOID
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_diag_subtype(p_name CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_diag_test(p_node_id INTEGER, p_diag_type_id INTEGER, p_diag_subtype_id INTEGER, p_start_time TIMESTAMP WITHOUT TIME ZONE, p_end_time TIMESTAMP WITHOUT TIME ZONE, p_component_index INTEGER, p_test_result_id INTEGER) RETURNS VOID
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_diag_test_config(p_node_id INTEGER, p_diag_type_id INTEGER, p_diag_subtype_id INTEGER, p_start_time TIMESTAMP WITHOUT TIME ZONE, p_test_param_id INTEGER, p_value_int BIGINT, p_value_real DOUBLE PRECISION, p_value_str CHARACTER VARYING, p_units CHARACTER VARYING) RETURNS VOID
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_diag_type(p_name CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_feature(p_name CHARACTER VARYING, p_data_type_id INTEGER) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_node(p_hostname CHARACTER VARYING) RETURNS INTEGER
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

        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_node_feature(p_node_id INTEGER, p_feature_id INTEGER, p_value_int BIGINT, p_value_real DOUBLE PRECISION, p_value_str CHARACTER VARYING, p_units CHARACTER VARYING) RETURNS VOID
            LANGUAGE plpgsql
            AS $$
        BEGIN
            INSERT INTO node_feature(
                node_id,
                feature_id,
                value_int,
                value_real,
                value_str,
                units)
            VALUES(
                p_node_id,
                p_feature_id,
                p_value_int,
                p_value_real,
                p_value_str,
                p_units);

            RETURN;
        END
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_test_param(p_name CHARACTER VARYING, p_data_type_id INTEGER) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_test_result(p_name CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION alert_db_size(max_db_size_in_gb DOUBLE PRECISION, log_level_percentage DOUBLE PRECISION, warning_level_percentage DOUBLE PRECISION, critical_level_percentage DOUBLE PRECISION, log_tag TEXT) RETURNS TEXT
            LANGUAGE plpgsql VOLATILE
            AS $$
        -- Query the current database for its size and compare with the
        -- specified arguments to raise the corresponding message.
        -- Parameter size restriction:
        --     0 < max_db_size_in_gb
        --     0 <= notice < warning < critical <= 1.0
        -- Note:  Enable \"log_destination='syslog'\" in postgresql.conf to log to syslog
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION data_type_exists(p_data_type_id INTEGER) RETURNS BOOLEAN
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION diag_exists(p_diag_type CHARACTER VARYING, p_diag_subtype CHARACTER VARYING) RETURNS BOOLEAN
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION diag_exists(p_diag_type_id INTEGER, p_diag_subtype_id INTEGER) RETURNS BOOLEAN
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION generate_partition_triggers_ddl(table_name TEXT, time_stamp_column_name TEXT, partition_interval TEXT, number_of_interval_of_data_to_keep INTEGER DEFAULT 180)RETURNS TEXT
            LANGUAGE plpgsql IMMUTABLE STRICT
            AS $$
        DECLARE
            ddl_script TEXT;
            timestamp_mask_by_interval TEXT;
            timestamp_mask_length INTEGER;
            insert_stmt TEXT;
            table_name_from_arg ALIAS FOR table_name;
            insert_stmt_position INTEGER;
            insert_stmt_column_name TEXT;
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
                    EXECUTE(''DROP TABLE IF EXISTS '' || quote_ident(''' || table_name || '_'' || to_char(NEW.' || time_stamp_column_name || ' - INTERVAL ''' || number_of_interval_of_data_to_keep || ' ' || partition_interval || ''', ''' || timestamp_mask_by_interval || ''') || ''_' || time_stamp_column_name || ''') || '';'');

                    -- Clean up any left over partitions that were missed by the DROP statement above
                    ---- Step 1 of 2:  Delete left over tuples that are older than the number of tuples to keep
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
                            EXECUTE(''DROP TABLE IF EXISTS '' || QUOTE_IDENT(table_partition.partition_name) || '';'');
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION get_data_item_id(p_data_item CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION get_diag_subtype_id(p_diag_subtype CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION get_diag_type_id(p_diag_type CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION get_feature_id(p_feature CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION get_node_id(p_hostname CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION get_test_param_id(p_name CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION get_test_result_id(p_test_result CHARACTER VARYING) RETURNS INTEGER
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION node_feature_exists(p_node_id INTEGER, p_feature_id INTEGER) RETURNS BOOLEAN
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
        $$;"""))
    op.execute(textwrap.dedent("""CREATE OR REPLACE FUNCTION purge_data(name_of_table TEXT, name_of_timestamp_column TEXT, number_of_time_unit_from_now INTEGER, time_unit TEXT) RETURNS VOID
            LANGUAGE plpgsql VOLATILE
            AS $$
        BEGIN
            EXECUTE('DELETE FROM ' || name_of_table || ' WHERE ' || name_of_table || '.' || name_of_timestamp_column || ' < CURRENT_TIMESTAMP - interval ''' || number_of_time_unit_from_now || ' ' || time_unit || ''';');
        END
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION record_data_sample(p_hostname CHARACTER VARYING, p_data_group CHARACTER VARYING, p_data_item CHARACTER VARYING, p_time_stamp TIMESTAMP WITHOUT TIME ZONE, p_data_type_id INTEGER, p_value_int BIGINT, p_value_real DOUBLE PRECISION, p_value_str CHARACTER VARYING, p_units CHARACTER VARYING) RETURNS VOID
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION record_diag_test_config(p_hostname CHARACTER VARYING, p_diag_type CHARACTER VARYING, p_diag_subtype CHARACTER VARYING, p_start_time TIMESTAMP WITHOUT TIME ZONE, p_test_param CHARACTER VARYING, p_data_type_id INTEGER, p_value_int BIGINT, p_value_real DOUBLE PRECISION, p_value_str CHARACTER VARYING, p_units CHARACTER VARYING) RETURNS VOID
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION record_diag_test_result(p_hostname CHARACTER VARYING, p_diag_type CHARACTER VARYING, p_diag_subtype CHARACTER VARYING, p_start_time TIMESTAMP WITHOUT TIME ZONE, p_end_time TIMESTAMP WITHOUT TIME ZONE, p_component_index INTEGER, p_test_result CHARACTER VARYING) RETURNS VOID
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
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION set_node_feature(p_hostname CHARACTER VARYING, p_feature CHARACTER VARYING, p_data_type_id INTEGER, p_value_int BIGINT, p_value_real DOUBLE PRECISION, p_value_str CHARACTER VARYING, p_units CHARACTER VARYING) RETURNS VOID
            LANGUAGE plpgsql
            AS $$
        DECLARE
            v_node_id INTEGER := 0;
            v_feature_id INTEGER := 0;
            v_node_feature_exists BOOLEAN := TRUE;
        BEGIN
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
                -- If so, just update it
                PERFORM update_node_feature(
                    v_node_id,
                    v_feature_id,
                    p_value_int,
                    p_value_real,
                    p_value_str,
                    p_units);
            ELSE
                -- If not, add it
                PERFORM add_node_feature(
                    v_node_id,
                    v_feature_id,
                    p_value_int,
                    p_value_real,
                    p_value_str,
                    p_units);
            END IF;

            RETURN;
        END
        $$;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION update_node_feature(p_node_id INTEGER, p_feature_id INTEGER, p_value_int BIGINT, p_value_real DOUBLE PRECISION, p_value_str CHARACTER VARYING, p_units CHARACTER VARYING) RETURNS VOID
            LANGUAGE plpgsql
            AS $$
        BEGIN
            UPDATE node_feature
            SET value_int = p_value_int,
                value_real = p_value_real,
                value_str = p_value_str,
                units = p_units
            WHERE node_id = p_node_id AND feature_id = p_feature_id;
        END
        $$;"""))


def _postgresql_downgrade_ddl():
    op.execute("DROP FUNCTION IF EXISTS update_node_feature(INTEGER, INTEGER, BIGINT, DOUBLE PRECISION, CHARACTER VARYING, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS set_node_feature(CHARACTER VARYING, CHARACTER VARYING, INTEGER, BIGINT, DOUBLE PRECISION, CHARACTER VARYING, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS record_diag_test_result(CHARACTER VARYING, CHARACTER VARYING, CHARACTER VARYING, TIMESTAMP WITHOUT TIME ZONE, TIMESTAMP WITHOUT TIME ZONE, INTEGER, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS record_diag_test_config(CHARACTER VARYING, CHARACTER VARYING, CHARACTER VARYING, TIMESTAMP WITHOUT TIME ZONE, CHARACTER VARYING, INTEGER, BIGINT, DOUBLE PRECISION, CHARACTER VARYING, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS record_data_sample(CHARACTER VARYING, CHARACTER VARYING, CHARACTER VARYING, TIMESTAMP WITHOUT TIME ZONE, INTEGER, BIGINT, DOUBLE PRECISION, CHARACTER VARYING, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS purge_data(TEXT, TEXT, INTEGER, TEXT)")
    op.execute("DROP FUNCTION IF EXISTS node_feature_exists(INTEGER, INTEGER)")
    op.execute("DROP FUNCTION IF EXISTS get_test_result_id(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS get_test_param_id(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS get_node_id(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS get_feature_id(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS get_diag_type_id(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS get_diag_subtype_id(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS get_data_item_id(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS generate_partition_triggers_ddl(TEXT, TEXT, TEXT, INTEGER)")
    op.execute("DROP FUNCTION IF EXISTS diag_exists(INTEGER, INTEGER)")
    op.execute("DROP FUNCTION IF EXISTS diag_exists(CHARACTER VARYING, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS data_type_exists(INTEGER)")
    op.execute("DROP FUNCTION IF EXISTS alert_db_size(DOUBLE PRECISION, DOUBLE PRECISION, DOUBLE PRECISION, DOUBLE PRECISION, TEXT)")
    op.execute("DROP FUNCTION IF EXISTS add_test_result(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS add_test_param(CHARACTER VARYING, INTEGER)")
    op.execute("DROP FUNCTION IF EXISTS add_node_feature(INTEGER, INTEGER, BIGINT, DOUBLE PRECISION, CHARACTER VARYING, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS add_node(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS add_feature(CHARACTER VARYING, INTEGER)")
    op.execute("DROP FUNCTION IF EXISTS add_diag_type(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS add_diag_test_config(INTEGER, INTEGER, INTEGER, TIMESTAMP WITHOUT TIME ZONE, INTEGER, BIGINT, DOUBLE PRECISION, CHARACTER VARYING, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS add_diag_test(INTEGER, INTEGER, INTEGER, TIMESTAMP WITHOUT TIME ZONE, TIMESTAMP WITHOUT TIME ZONE, INTEGER, INTEGER)")
    op.execute("DROP FUNCTION IF EXISTS add_diag_subtype(CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS add_diag(INTEGER, INTEGER)")
    op.execute("DROP FUNCTION IF EXISTS add_data_type(INTEGER, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS add_data_sample(INTEGER, INTEGER, TIMESTAMP WITHOUT TIME ZONE, BIGINT, DOUBLE PRECISION, CHARACTER VARYING, CHARACTER VARYING)")
    op.execute("DROP FUNCTION IF EXISTS add_data_item(CHARACTER VARYING, INTEGER)")

    op.execute("DROP VIEW IF EXISTS node_features_view;")
    op.execute("DROP VIEW IF EXISTS data_samples_view;")


def upgrade():
    op.create_table('data_type',
        sa.Column('data_type_id', sa.Integer(), nullable=False, unique=True),
        sa.Column('name', sa.String(length=150)),
        sa.PrimaryKeyConstraint('data_type_id', name=op.f('pk_data_type')),
        sa.UniqueConstraint('name', name=op.f('uq_data_type_name')))
    op.create_table('diag_subtype',
        sa.Column('diag_subtype_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=250), nullable=False),
        sa.PrimaryKeyConstraint('diag_subtype_id', name=op.f('pk_diag_subtype')),
        sa.UniqueConstraint('name', name=op.f('uq_diag_subtype_name')))
    op.create_table('diag_type',
        sa.Column('diag_type_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=250), nullable=False),
        sa.PrimaryKeyConstraint('diag_type_id', name=op.f('pk_diag_type')),
        sa.UniqueConstraint('name', name=op.f('uq_diag_type_name')))
    op.create_table('event_type',
        sa.Column('event_type_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=250)),
        sa.PrimaryKeyConstraint('event_type_id', name=op.f('pk_event_type')))
    op.create_table('fru_type',
        sa.Column('fru_type_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=250), nullable=False),
        sa.PrimaryKeyConstraint('fru_type_id', name=op.f('pk_fru_type')))
    op.create_table('job',
        sa.Column('job_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=100)),
        sa.Column('num_nodes', sa.Integer(), nullable=False),
        sa.Column('username', sa.String(length=75), nullable=False),
        sa.Column('time_submitted', sa.DateTime(), nullable=False),
        sa.Column('start_time', sa.DateTime()),
        sa.Column('end_time', sa.DateTime()),
        sa.Column('energy_usage', sa.Float()),
        sa.Column('exit_status', sa.Integer()),
        sa.PrimaryKeyConstraint('job_id', name=op.f('pk_job')))
    op.create_table('node',
        sa.Column('node_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('hostname', sa.String(length=256), nullable=False),
        sa.Column('node_type', sa.SmallInteger()),
        sa.Column('status', sa.SmallInteger()),
        sa.Column('num_cpus', sa.SmallInteger()),
        sa.Column('num_sockets', sa.SmallInteger()),
        sa.Column('cores_per_socket', sa.SmallInteger()),
        sa.Column('threads_per_core', sa.SmallInteger()),
        sa.Column('memory', sa.SmallInteger()),
        sa.PrimaryKeyConstraint('node_id', name=op.f('pk_node')),
        sa.UniqueConstraint('hostname', name=op.f('uq_node_hostname')))
    op.create_table('test_result',
        sa.Column('test_result_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=250), nullable=False),
        sa.PrimaryKeyConstraint('test_result_id', name=op.f('pk_test_result')),
        sa.UniqueConstraint('name', name=op.f('uq_test_result_name')))
    op.create_table('data_item',
        sa.Column('data_item_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=250), nullable=False),
        sa.Column('data_type_id', sa.Integer(), nullable=False),
        sa.ForeignKeyConstraint(['data_type_id'], ['data_type.data_type_id'], name=op.f('fk_data_item_data_type_id_data_type')),
        sa.PrimaryKeyConstraint('data_item_id', name=op.f('pk_data_item')),
        sa.UniqueConstraint('name', name=op.f('uq_data_item_name')))
    op.create_table('diag',
        sa.Column('diag_type_id', sa.Integer(), nullable=False),
        sa.Column('diag_subtype_id', sa.Integer(), nullable=False),
        sa.ForeignKeyConstraint(['diag_subtype_id'], ['diag_subtype.diag_subtype_id'], name=op.f('fk_diag_diag_subtype_id_diag_subtype')),
        sa.ForeignKeyConstraint(['diag_type_id'], ['diag_type.diag_type_id'], name=op.f('fk_diag_diag_type_id_diag_type')),
        sa.PrimaryKeyConstraint('diag_type_id', 'diag_subtype_id', name=op.f('pk_diag')))
    op.create_table('event',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('event_type_id', sa.Integer(), nullable=False),
        sa.Column('time_stamp', sa.DateTime(), nullable=False),
        sa.Column('description', sa.Text(), nullable=False),
        sa.ForeignKeyConstraint(['event_type_id'], ['event_type.event_type_id'], name=op.f('fk_event_event_type_id_event_type')),
        sa.ForeignKeyConstraint(['node_id'], ['node.node_id'], name=op.f('fk_event_node_id_node')),
        sa.PrimaryKeyConstraint('node_id', 'event_type_id', 'time_stamp', name=op.f('pk_event')))
    op.create_table('feature',
        sa.Column('feature_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=250), nullable=False),
        sa.Column('data_type_id', sa.Integer(), nullable=False),
        sa.ForeignKeyConstraint(['data_type_id'], ['data_type.data_type_id'], name=op.f('fk_feature_data_type_id_data_type')),
        sa.PrimaryKeyConstraint('feature_id', name=op.f('pk_feature')),
        sa.UniqueConstraint('name', name=op.f('uq_feature_name')))
    op.create_table('fru',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('fru_type_id', sa.Integer(), nullable=False),
        sa.Column('fru_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('serial_number', sa.String(length=50), nullable=False),
        sa.ForeignKeyConstraint(['fru_type_id'], ['fru_type.fru_type_id'], name=op.f('fk_fru_fru_type_id_fru_type')),
        sa.ForeignKeyConstraint(['node_id'], ['node.node_id'], name=op.f('fk_fru_node_id_node')),
        sa.PrimaryKeyConstraint('node_id', 'fru_type_id', 'fru_id', name=op.f('pk_fr')))
    op.create_table('job_node',
        sa.Column('job_id', sa.Integer(), nullable=False),
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.ForeignKeyConstraint(['job_id'], ['job.job_id'], name=op.f('fk_job_node_job_id_job')),
        sa.ForeignKeyConstraint(['node_id'], ['node.node_id'], name=op.f('fk_job_node_node_id_node')),
        sa.PrimaryKeyConstraint('job_id', 'node_id', name=op.f('pk_job_node')))
    op.create_table('test_param',
        sa.Column('test_param_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=250), nullable=False),
        sa.Column('data_type_id', sa.Integer(), nullable=False),
        sa.ForeignKeyConstraint(['data_type_id'], ['data_type.data_type_id'], name=op.f('fk_test_param_data_type_id_data_type')),
        sa.PrimaryKeyConstraint('test_param_id', name=op.f('pk_test_param')),
        sa.UniqueConstraint('name', name=op.f('uq_test_param_name')))
    op.create_table('data_sample',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('data_item_id', sa.Integer(), nullable=False),
        sa.Column('time_stamp', sa.DateTime(), nullable=False),
        sa.Column('value_int', sa.BigInteger()),
        sa.Column('value_real', sa.Float(precision=53)),
        sa.Column('value_str', sa.String(length=50)),
        sa.Column('units', sa.String(length=50)),
        sa.ForeignKeyConstraint(['data_item_id'], ['data_item.data_item_id'], name=op.f('fk_data_sample_data_item_id_data_item')),
        sa.ForeignKeyConstraint(['node_id'], ['node.node_id'], name=op.f('fk_data_sample_node_id_node')),
        sa.PrimaryKeyConstraint('node_id', 'data_item_id', 'time_stamp', name=op.f('pk_data_sample')),
        sa.CheckConstraint('(value_int  IS NOT NULL OR value_real IS NOT NULL OR value_str  IS NOT NULL)', name=op.f('ck_data_sample_at_least_one_value')))
    op.create_table('diag_test',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('diag_type_id', sa.Integer(), nullable=False),
        sa.Column('diag_subtype_id', sa.Integer(), nullable=False),
        sa.Column('start_time', sa.DateTime(), nullable=False),
        sa.Column('end_time', sa.DateTime()),
        sa.Column('component_index', sa.Integer()),
        sa.Column('test_result_id', sa.Integer(), nullable=False),
        sa.ForeignKeyConstraint(['diag_type_id', 'diag_subtype_id'], ['diag.diag_type_id', 'diag.diag_subtype_id'], name=op.f('fk_diag_test_diag_type_id_diag')),
        sa.ForeignKeyConstraint(['node_id'], ['node.node_id'], name=op.f('fk_diag_test_node_id_node')),
        sa.PrimaryKeyConstraint('node_id', 'diag_type_id', 'diag_subtype_id', 'start_time', name=op.f('pk_diag_test')))
    op.create_table('maintenance_record',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('fru_type_id', sa.Integer(), nullable=False),
        sa.Column('fru_id', sa.Integer(), nullable=False),
        sa.Column('replacement_date', sa.Date(), nullable=False),
        sa.Column('old_serial_number', sa.String(length=50), nullable=False),
        sa.Column('new_serial_number', sa.String(length=50), nullable=False),
        sa.ForeignKeyConstraint(['node_id', 'fru_type_id', 'fru_id'], ['fru.node_id', 'fru.fru_type_id', 'fru.fru_id'], name=op.f('fk_maintenance_record_node_id_fr')),
        sa.PrimaryKeyConstraint('node_id', 'fru_type_id', 'fru_id', 'replacement_date', name=op.f('pk_maintenance_record')))
    op.create_table('node_feature',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('feature_id', sa.Integer(), nullable=False),
        sa.Column('value_real', sa.Float(precision=53)),
        sa.Column('value_str', sa.String(length=100)),
        sa.Column('units', sa.String(length=50)),
        sa.Column('value_int', sa.BigInteger()),
        sa.ForeignKeyConstraint(['feature_id'], ['feature.feature_id'], name=op.f('fk_node_feature_feature_id_feature')),
        sa.ForeignKeyConstraint(['node_id'], ['node.node_id'], name=op.f('fk_node_feature_node_id_node')),
        sa.PrimaryKeyConstraint('node_id', 'feature_id', name=op.f('pk_node_feature')))
    op.create_table('diag_test_config',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('diag_type_id', sa.Integer(), nullable=False),
        sa.Column('diag_subtype_id', sa.Integer(), nullable=False),
        sa.Column('start_time', sa.DateTime(), nullable=False),
        sa.Column('test_param_id', sa.Integer(), nullable=False),
        sa.Column('value_real', sa.Float(precision=53)),
        sa.Column('value_str', sa.String(length=100)),
        sa.Column('units', sa.String(length=50)),
        sa.Column('value_int', sa.BigInteger()),
        sa.ForeignKeyConstraint(['node_id', 'diag_type_id', 'diag_subtype_id', 'start_time'], ['diag_test.node_id', 'diag_test.diag_type_id', 'diag_test.diag_subtype_id', 'diag_test.start_time'], name=op.f('fk_diag_test_config_node_id_diag_test')),
        sa.ForeignKeyConstraint(['test_param_id'], ['test_param.test_param_id'], name=op.f('fk_diag_test_config_test_param_id_test_param')),
        sa.PrimaryKeyConstraint('node_id', 'diag_type_id', 'diag_subtype_id', 'start_time', 'test_param_id', name=op.f('pk_diag_test_config')),
        sa.CheckConstraint('(value_int  IS NOT NULL OR value_real IS NOT NULL OR value_str  IS NOT NULL)', name=op.f('ck_diag_test_config_at_least_one_value')))
    op.create_table('data_sample_raw',
        sa.Column('hostname', sa.String(length=256), nullable=False),
        sa.Column('data_item', sa.String(length=250), nullable=False),
        sa.Column('time_stamp', sa.DateTime(), nullable=False),
        sa.Column('value_int', sa.BigInteger()),
        sa.Column('value_real', sa.Float(precision=53)),
        sa.Column('value_str', sa.String(length=50)),
        sa.Column('units', sa.String(length=50)),
        sa.Column('data_type_id', sa.Integer(), nullable=False),
        sa.CheckConstraint('(value_int  IS NOT NULL OR value_real IS NOT NULL OR value_str  IS NOT NULL)', name=op.f('ck_data_sample_raw_at_least_one_value')))

    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_upgrade_ddl()
    else:
        print("Views were not created.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return


def downgrade():
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_downgrade_ddl()
    else:
        print("Views were not dropped.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return

    op.drop_table('data_sample_raw')
    op.drop_table('diag_test_config')
    op.drop_table('node_feature')
    op.drop_table('maintenance_record')
    op.drop_table('diag_test')
    op.drop_table('data_sample')
    op.drop_table('test_param')
    op.drop_table('job_node')
    op.drop_table('fru')
    op.drop_table('feature')
    op.drop_table('event')
    op.drop_table('diag')
    op.drop_table('data_item')
    op.drop_table('test_result')
    op.drop_table('node')
    op.drop_table('job')
    op.drop_table('fru_type')
    op.drop_table('event_type')
    op.drop_table('diag_type')
    op.drop_table('diag_subtype')
    op.drop_table('data_type')
