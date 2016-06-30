#
# Copyright (c) 2016 Intel Inc. All rights reserved
#
"""functions for queries

Revision ID: d109117b1990
Revises: f6d80b502c89
Create Date: 2016-06-07 14:31:54.260056

"""

# revision identifiers, used by Alembic.
revision = 'd109117b1990'
down_revision = 'f6d80b502c89'
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap

def _postgresql_upgrade_ddl():
    _postgresql_create_query_funcs()
    _postgresql_remove_old_views()

def _postgresql_downgrade_ddl():
    _postgresql_remove_query_funcs()
    _postgresql_create_old_views()

def _postgresql_create_query_funcs():
    """
    Creates the following functions to make queries in a more effective way:
        is_numeric(TEXT) RETURNS BOOLEAN
        orcm_comma_list_to_regex(list TEXT, comma BOOLEAN DEFAULT TRUE) RETURNS TEXT
        query_event_data(ts_from TIMESTAMP, ts_to TIMESTAMP, nodes TEXT, num_limit INT) RETURNS TABLE
        query_event_snsr_data(id_event INT, search_interval INTERVAL, sensors TEXT, nodes TEXT, num_limit INT) RETURNS TABLE
        query_idle(idle_interval INTERVAL, nodes TEXT, num_limit INT) RETURNS TABLE
        query_log(val TEXT, ts_from TIMESTAMP, ts_to TIMESTAMP, nodes TEXT, num_limit INT) RETURNS TABLE
        query_node_status(nodes TEXT, num_limit INT) RETURNS TABLE
        query_sensor_history(sensors TEXT, ts_from TIMESTAMP, ts_to TIMESTAMP, val_from TEXT, val_to TEXT, nodes TEXT, num_limit INT) RETURNS TABLE
        query_snsr_get_inventory(nodes TEXT, num_limit INT) RETURNS TABLE
    """
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION is_numeric(TEXT)
        RETURNS BOOLEAN AS
        $func$
        DECLARE
            num NUMERIC;
        BEGIN
            num := $1::NUMERIC;
            RETURN TRUE;
            EXCEPTION WHEN OTHERS THEN
                RETURN FALSE;
        END
        $func$
        STRICT LANGUAGE plpgsql IMMUTABLE;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION orcm_comma_list_to_regex(list TEXT, comma BOOLEAN DEFAULT TRUE)
        RETURNS TEXT AS
        $func$
        BEGIN
            IF( list IS NULL ) THEN
                list := '.*';
            ELSE
                list := REGEXP_REPLACE(list, E'([[\\\\](){}.+^$|\\\\\\\\?-])', E'\\\\\\\\\\\\1', 'g');
                list := REGEXP_REPLACE(list, E'\\\\*', '.*','g');
                IF( comma IS TRUE ) THEN
                    list := '(^' || REGEXP_REPLACE(list, ',', '$)|(^', 'g') || '$)';
                ELSE
                    list := '^' || list || '$';
                END IF;
            END IF;

            RETURN list;
        END
        $func$  LANGUAGE plpgsql;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION query_event_data(ts_from TIMESTAMP, ts_to TIMESTAMP, nodes TEXT, num_limit INT)
        RETURNS TABLE (
            "EVENT ID" TEXT,
            "TIME" TEXT,
            "SEVERITY" TEXT,
            "TYPE" TEXT,
            "HOSTNAME" TEXT,
            "MESSAGE" TEXT) AS
        $func$
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
        $func$  LANGUAGE plpgsql;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION query_event_snsr_data(id_event INT, search_interval INTERVAL, sensors TEXT, nodes TEXT, num_limit INT)
        RETURNS TABLE (
            "TIME" TEXT,
            "HOSTNAME" TEXT,
            "DATA ITEM" TEXT,
            "VALUE" TEXT,
            "UNITS" TEXT) AS
        $func$
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
        $func$  LANGUAGE plpgsql;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION query_idle(idle_interval INTERVAL, nodes TEXT, num_limit INT)
        RETURNS TABLE (
            "HOSTNAME" TEXT,
            "IDLE TIME" TEXT) AS
        $func$
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
        $func$  LANGUAGE plpgsql;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION query_log(val TEXT, ts_from TIMESTAMP, ts_to TIMESTAMP, nodes TEXT, num_limit INT)
        RETURNS TABLE (
            "HOSTNAME" TEXT,
            "DATA ITEM" TEXT,
            "TIME" TEXT,
            "MESSAGE" TEXT) AS
        $func$
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
        $func$  LANGUAGE plpgsql;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION query_node_status(nodes TEXT, num_limit INT)
        RETURNS TABLE (
            "HOSTNAME" TEXT,
            "STATUS" TEXT) AS
        $func$
        BEGIN
            nodes := orcm_comma_list_to_regex(nodes);

            RETURN QUERY
            SELECT n.hostname::TEXT, n.status::TEXT
            FROM node n
            WHERE n.hostname ~* nodes
            LIMIT num_limit;
        END
        $func$  LANGUAGE plpgsql;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION query_sensor_history(sensors TEXT, ts_from TIMESTAMP, ts_to TIMESTAMP, val_from TEXT, val_to TEXT, nodes TEXT, num_limit INT)
        RETURNS TABLE (
            "TIME" TEXT,
            "HOSTNAME" TEXT,
            "DATA ITEM" TEXT,
            "VALUE" TEXT,
            "UNITS" TEXT) AS
        $func$
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
        $func$  LANGUAGE plpgsql;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION query_snsr_get_inventory(nodes TEXT, num_limit INT)
        RETURNS TABLE (
            "HOSTNAME" TEXT,
            "PLUGIN NAME" TEXT,
            "SENSOR NAME" TEXT) AS
        $func$
        BEGIN
            nodes := orcm_comma_list_to_regex(nodes);

            RETURN QUERY
            SELECT n.hostname::TEXT, REGEXP_REPLACE(f.name::TEXT, '(^sensor_)|(_\\d+$)','','gi'),
                COALESCE(nf.value_int::TEXT, nf.value_real::TEXT, nf.value_str::TEXT)
            FROM node_feature nf INNER JOIN node n ON n.hostname ~* nodes
                AND n.node_id = nf.node_id
            INNER JOIN feature f ON f.name ~~* 'sensor%' AND f.feature_id = nf.feature_id
            LIMIT num_limit;
        END
        $func$  LANGUAGE plpgsql;
    """))

def _postgresql_remove_query_funcs():
    """
    Drops the following functions that were intented to make queries effective:
        is_numeric(TEXT) RETURNS BOOLEAN
        orcm_comma_list_to_regex(list TEXT, comma BOOLEAN DEFAULT TRUE) RETURNS TEXT
        query_event_data(ts_from TIMESTAMP, ts_to TIMESTAMP, nodes TEXT, num_limit INT) RETURNS TABLE
        query_event_snsr_data(id_event INT, search_interval INTERVAL, sensors TEXT, nodes TEXT, num_limit INT) RETURNS TABLE
        query_idle(idle_interval INTERVAL, nodes TEXT, num_limit INT) RETURNS TABLE
        query_log(val TEXT, ts_from TIMESTAMP, ts_to TIMESTAMP, nodes TEXT, num_limit INT) RETURNS TABLE
        query_node_status(nodes TEXT, num_limit INT) RETURNS TABLE
        query_sensor_history(sensors TEXT, ts_from TIMESTAMP, ts_to TIMESTAMP, val_from TEXT, val_to TEXT, nodes TEXT, num_limit INT) RETURNS TABLE
        query_snsr_get_inventory(nodes TEXT, num_limit INT) RETURNS TABLE
    """

    op.execute(textwrap.dedent("DROP FUNCTION IF EXISTS query_event_data(ts_from TIMESTAMP, ts_to TIMESTAMP, nodes TEXT, num_limit INT);"))
    op.execute(textwrap.dedent("DROP FUNCTION IF EXISTS query_event_snsr_data(id_event INT, search_interval INTERVAL, sensors TEXT, nodes TEXT, num_limit INT);"))
    op.execute(textwrap.dedent("DROP FUNCTION IF EXISTS query_idle(idle_interval INTERVAL, nodes TEXT, num_limit INT);"))
    op.execute(textwrap.dedent("DROP FUNCTION IF EXISTS query_log(val TEXT, ts_from TIMESTAMP, ts_to TIMESTAMP, nodes TEXT, num_limit INT);"))
    op.execute(textwrap.dedent("DROP FUNCTION IF EXISTS query_node_status(nodes TEXT, num_limit INT);"))
    op.execute(textwrap.dedent("DROP FUNCTION IF EXISTS query_sensor_history(sensors TEXT, ts_from TIMESTAMP, ts_to TIMESTAMP, val_from TEXT, val_to TEXT, nodes TEXT, num_limit INT);"))
    op.execute(textwrap.dedent("DROP FUNCTION IF EXISTS query_snsr_get_inventory(nodes TEXT, num_limit INT);"))
    op.execute(textwrap.dedent("DROP FUNCTION IF EXISTS is_numeric(TEXT);"))
    op.execute(textwrap.dedent("DROP FUNCTION IF EXISTS orcm_comma_list_to_regex(list TEXT, comma BOOLEAN);"))

def _postgresql_create_old_views():
    """
    Creates the following views that were used to make queries:
        data_samples_view
        data_sensors_view
        event_date_view
        event_sensor_data_view
        event_view
        nodes_idle_time_view
        syslog_view
    """

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW data_samples_view AS
        SELECT data_sample_raw.hostname, data_sample_raw.data_item,
            data_sample_raw.data_type_id, data_sample_raw.time_stamp,
            data_sample_raw.value_int, data_sample_raw.value_real,
            data_sample_raw.value_str, data_sample_raw.units
        FROM data_sample_raw;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW data_sensors_view AS
        SELECT dsr.hostname, dsr.data_item,
            concat(dsr.time_stamp) AS time_stamp,
            concat(dsr.value_int, dsr.value_real, dsr.value_str) AS value_str,
            dsr.units
        FROM data_sample_raw dsr;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW event_date_view AS
        SELECT event.event_id::text AS event_id,
            event.time_stamp::text AS time_stamp
        FROM event;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW event_sensor_data_view AS
        SELECT data_sample_raw.time_stamp::text AS time_stamp,
            data_sample_raw.hostname, data_sample_raw.data_item,
            concat(data_sample_raw.value_int::text, data_sample_raw.value_real::text, data_sample_raw.value_str) AS value,
            data_sample_raw.units, data_sample_raw.time_stamp AS time_stamp2
        FROM data_sample_raw;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW event_view AS
        SELECT concat(event.event_id) AS event_id,
            concat(event.time_stamp) AS time_stamp, event.severity, event.type,
            event_data.value_str AS hostname,
            get_event_msg(event.event_id) AS event_message
        FROM event JOIN event_data ON event.event_id = event_data.event_id
            AND event_data.event_data_key_id = 1;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW nodes_idle_time_view AS
        SELECT data_sample_raw.hostname,
            max(data_sample_raw.time_stamp) AS last_reading,
            now() - max(data_sample_raw.time_stamp)::timestamp with time zone AS idle_time,
            concat(now() - max(data_sample_raw.time_stamp)::timestamp with time zone) AS idle_time_str
        FROM data_sample_raw
        GROUP BY data_sample_raw.hostname;
    """))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW syslog_view AS
        SELECT data_sample_raw.hostname, data_sample_raw.data_item,
            data_sample_raw.time_stamp::text AS time_stamp,
            data_sample_raw.value_str AS log
        FROM data_sample_raw
        WHERE data_sample_raw.data_item::text ~~ 'syslog_log%'::text;
    """))

def _postgresql_remove_old_views():
    """
    Drops the following views that were used to make queries:
        data_samples_view
        data_sensors_view
        event_date_view
        event_sensor_data_view
        event_view
        nodes_idle_time_view
        syslog_view
    """
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS data_samples_view;"))
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS data_sensors_view;"))
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS event_date_view;"))
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS event_sensor_data_view;"))
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS event_view;"))
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS nodes_idle_time_view;"))
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS syslog_view;"))

def upgrade():
    """
    Creates several functions to optimize the way in which the queries are made, making
    them faster and adding stability on the parameters by validating them and making
    some of them optional in an easier way.
    Also adds the posibility of giving a list of sensors as argument (previously,
    only one was supported).
    """
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_upgrade_ddl()
    else:
        print("Upgrade unsuccessful. "
            "'%s' is not a supported database dialect." % db_dialect.name)

    return

def downgrade():
    """
    Deletes the functions created to optimize queries.
    """
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_downgrade_ddl()
    else:
        print("Downgrade unsuccessful. "
            "'%s' is not a supported database dialect." % db_dialect.name)

    return
