#
# Copyright (c) 2016 Intel Inc. All rights reserved
#
"""Added Inventory timestamp

Added 'time_stamp' column to node_feature
Updated add_node_feature(), update_node_feature(), and set_node_feature() to
include the new 'time_stamp' field.

Revision ID: 36357b09c750
Revises: 6ff60a50de68
Create Date: 02/26/2016 13:30:00.923134

"""

# revision identifiers, used by Alembic.
revision = '36357b09c750'
down_revision = '6ff60a50de68'
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap


def _postgresql_upgrade_ddl():
    """Update stored procedures after added column"""
    op.execute(textwrap.dedent("""
        DROP FUNCTION add_node_feature(
            integer, 
            integer, 
            bigint, 
            double precision, 
            character varying, 
            character varying);"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_node_feature(
                p_node_id INTEGER,
                p_feature_id INTEGER,
                p_value_int BIGINT,
                p_value_real DOUBLE PRECISION,
                p_value_str CHARACTER VARYING,
                p_units CHARACTER VARYING,
                p_time_stamp TIMESTAMP WITHOUT TIME ZONE)
            RETURNS VOID AS
        $BODY$
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
        $BODY$
            LANGUAGE plpgsql
            COST 100;"""))
    op.execute(textwrap.dedent("""
        DROP FUNCTION update_node_feature(
            integer, 
            integer, 
            bigint, 
            double precision, 
            character varying, 
            character varying);"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION update_node_feature(
                p_node_id INTEGER,
                p_feature_id INTEGER,
                p_value_int BIGINT,
                p_value_real DOUBLE PRECISION,
                p_value_str CHARACTER VARYING,
                p_units CHARACTER VARYING,
                p_time_stamp TIMESTAMP WITHOUT TIME ZONE)
            RETURNS VOID AS
        $BODY$
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
        $BODY$
            LANGUAGE plpgsql
            COST 100;"""))
    op.execute(textwrap.dedent("""
        DROP FUNCTION set_node_feature(
            character varying, 
            character varying, 
            integer, 
            bigint, 
            double precision, 
            character varying, 
            character varying);"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION set_node_feature(
                p_hostname CHARACTER VARYING,
                p_feature CHARACTER VARYING,
                p_data_type_id INTEGER,
                p_value_int BIGINT,
                p_value_real DOUBLE PRECISION,
                p_value_str CHARACTER VARYING,
                p_units CHARACTER VARYING,
                p_time_stamp TIMESTAMP WITHOUT TIME ZONE)
            RETURNS VOID AS
        $BODY$
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
        $BODY$
            LANGUAGE plpgsql VOLATILE
            COST 100;"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW node_features_view AS
        SELECT
            node_feature.node_id::TEXT AS node_id,
            node.hostname,
            node_feature.feature_id::TEXT AS feature_id,
            feature.name AS feature,
            COALESCE(''::TEXT) AS value_int,
            COALESCE(''::TEXT) AS value_real,
            concat(node_feature.value_int::TEXT, node_feature.value_real::TEXT, node_feature.value_str) AS value_str,
            node_feature.units,
            CAST(node_feature.time_stamp AS TEXT) AS time_stamp
        FROM node_feature
            JOIN node ON node.node_id = node_feature.node_id
            JOIN feature ON feature.feature_id = node_feature.feature_id;"""))


def _postgresql_downgrade_part1_ddl():
    """Restore the stored procedures before removing the column"""
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS node_features_view;"))
    op.execute(textwrap.dedent("""
        DROP FUNCTION set_node_feature(
            character varying, 
            character varying, 
            integer, 
            bigint, 
            double precision, 
            character varying, 
            character varying, 
            timestamp without time zone);"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION set_node_feature(
                p_hostname CHARACTER VARYING,
                p_feature CHARACTER VARYING,
                p_data_type_id INTEGER,
                p_value_int BIGINT,
                p_value_real DOUBLE PRECISION,
                p_value_str CHARACTER VARYING,
                p_units CHARACTER VARYING)
            RETURNS VOID
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
        DROP FUNCTION update_node_feature(
            integer, 
            integer, 
            bigint, 
            double precision, 
            character varying, 
            character varying, 
            timestamp without time zone);"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION update_node_feature(
                p_node_id INTEGER,
                p_feature_id INTEGER,
                p_value_int BIGINT,
                p_value_real DOUBLE PRECISION,
                p_value_str CHARACTER VARYING,
                p_units CHARACTER VARYING)
            RETURNS VOID
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
    op.execute(textwrap.dedent("""
        DROP FUNCTION add_node_feature(
            integer, 
            integer, 
            bigint, 
            double precision, 
            character varying, 
            character varying, 
            timestamp without time zone);"""))
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE FUNCTION add_node_feature(
                p_node_id INTEGER,
                p_feature_id INTEGER,
                p_value_int BIGINT,
                p_value_real DOUBLE PRECISION,
                p_value_str CHARACTER VARYING,
                p_units CHARACTER VARYING)
            RETURNS VOID
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


def _postgresql_downgrade_part2_ddl():
    """Restore 'node_features_view' view"""
    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW node_features_view AS
        SELECT
            node_feature.node_id::TEXT AS node_id,
            node.hostname,
            node_feature.feature_id::TEXT AS feature_id,
            feature.name AS feature,
            COALESCE(''::TEXT) AS value_int,
            COALESCE(''::TEXT) AS value_real,
            concat(node_feature.value_int::TEXT, node_feature.value_real::TEXT, node_feature.value_str) AS value_str,
            node_feature.units
        FROM node_feature
            JOIN node ON node.node_id = node_feature.node_id
            JOIN feature ON feature.feature_id = node_feature.feature_id;"""))


def upgrade():
    """Add 'time_stamp' column to 'node_feature' table and update all
    dependencies effected by this change.
      1) Drop the dependent view
      2) Add the new column
      3) Update all dependent stored procedures and recreate the view
       to include the new column
    """

    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        op.execute(textwrap.dedent("DROP VIEW IF EXISTS node_features_view;"))
    else:
        print("View 'node_features_view' was not drop in prepare for adding "
              "new 'time_stamp' column to 'node_feature' table.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return
    op.add_column('node_feature', sa.Column('time_stamp',
                                            sa.DateTime(),
                                            nullable=False,
                                            server_default=sa.func.now()))
    if 'postgresql' in db_dialect.name:
        _postgresql_upgrade_ddl()
    else:
        print("Functions were not updated.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return


def downgrade():
    """Remove the 'time_stamp' column from 'node_feature' table and restore
    all dependencies effected by this change.
      1) Restore all dependent stored procedures
      2) Drop the 'time_stamp' column
      3) Restore the view
    """

    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_downgrade_part1_ddl()
    else:
        print("Functions were not downgraded.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return
    op.drop_column('node_feature', 'time_stamp')
    if 'postgresql' in db_dialect.name:
        _postgresql_downgrade_part2_ddl()
    else:
        print("Functions were not downgraded.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return
