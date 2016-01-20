#
# Copyright (c) 2016 Intel Inc. All rights reserved
#
"""Updated generate_partition_triggers to correctly generate INSERT statement
Updated trigger handler to return the new data to support INSERT RETURNING syntax

Revision ID: 27089c78df35
Revises: 3f96c40b580a
Create Date: 2016-01-03 13:27:07.127115

"""

# revision identifiers, used by Alembic.
revision = '27089c78df35'
down_revision = '3f96c40b580a'
branch_labels = None

from alembic import op
import textwrap


def _postgresql_upgrade_ddl():
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION generate_partition_triggers_ddl(
            table_name text,
            time_stamp_column_name text,
            partition_interval text,
            number_of_interval_of_data_to_keep integer DEFAULT 180)
          RETURNS text AS
        $BODY$
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
        $BODY$
        LANGUAGE plpgsql IMMUTABLE STRICT;"""))


def _postgresql_downgrade_ddl():
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION generate_partition_triggers_ddl(table_name TEXT, time_stamp_column_name TEXT, partition_interval TEXT, number_of_interval_of_data_to_keep INTEGER DEFAULT 180)
            RETURNS TEXT
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



def upgrade():
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_upgrade_ddl()
    else:
        print("Function generate_partition_triggers() was not updated.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return


def downgrade():
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_downgrade_ddl()
    else:
        print("Function generate_partition_triggers() was not downgraded.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return
