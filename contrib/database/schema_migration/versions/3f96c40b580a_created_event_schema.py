#
# Copyright (c) 2015 Intel Corporation. All rights reserved
#
"""Created 'event', 'event_data', and 'event_data_key' tables to store data from the EvGen framework.

Revision ID: 3f96c40b580a
Revises: 1d10ef20817b
Create Date: 2015-11-19 22:26:55.393954

"""

# revision identifiers, used by Alembic.
import textwrap

revision = '3f96c40b580a'
down_revision = '1d10ef20817b'
branch_labels = None

from alembic import op
import sqlalchemy as sa


def _postgresql_upgrade_ddl():
    """Create the two event related functions:  add_event() and add_event_data()
    """
    op.execute(textwrap.dedent("""
        CREATE FUNCTION add_event(
                p_time_stamp TIMESTAMP WITHOUT TIME ZONE,
                p_severity CHARACTER VARYING,
                p_type CHARACTER VARYING,
                p_version CHARACTER VARYING,
                p_vendor CHARACTER VARYING,
                p_description CHARACTER VARYING)
            RETURNS INTEGER AS
        $BODY$
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
        $BODY$
            LANGUAGE plpgsql VOLATILE
            COST 100;"""))

    # Adding a new column as primary key.  Alembic doesn't handle adding new
    # column to existing table as autoincrement.
    op.execute("ALTER TABLE data_sample_raw ADD COLUMN data_sample_id SERIAL;")

    # Update all existing records to have default values for the new columns
    op.execute(textwrap.dedent("""
        UPDATE data_sample_raw
            SET data_sample_id = DEFAULT,
            app_value_type_id = data_type_id;"""))

    op.execute(textwrap.dedent("""
        CREATE FUNCTION add_event_data(
                p_event_id INTEGER,
                p_key_name CHARACTER VARYING,
                p_app_value_type_id INTEGER,
                p_value_int BIGINT,
                p_value_real DOUBLE PRECISION,
                p_value_str CHARACTER VARYING,
                p_units CHARACTER VARYING)
            RETURNS VOID AS
        $BODY$
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
        $BODY$
            LANGUAGE plpgsql VOLATILE
            COST 100;"""))


def upgrade():
    """Replace the 'event' table with the new one, create 'event_data' and
    'event_data_key' tables.  Also create function related to these tables.
    """
    op.drop_table('event')
    op.drop_table('event_type')
    op.create_table('event',
                    sa.Column('event_id', sa.Integer(), nullable=False,
                              autoincrement=True),
                    sa.Column('time_stamp', sa.DateTime(), nullable=False,
                              server_default=sa.func.now()),
                    sa.Column('severity', sa.String(length=50), nullable=False,
                              server_default=''),
                    sa.Column('type', sa.String(length=50), nullable=False,
                              server_default=''),
                    sa.Column('vendor', sa.String(length=250), nullable=False,
                              server_default=''),
                    sa.Column('version', sa.String(length=250), nullable=False,
                              server_default=''),
                    sa.Column('description', sa.Text(), nullable=False,
                              server_default=''),
                    sa.PrimaryKeyConstraint('event_id', name=op.f('pk_event')))
    op.create_table('event_data_key',
                    sa.Column('event_data_key_id', sa.Integer(),
                              nullable=False, autoincrement=True),
                    sa.Column('name', sa.String(length=250), nullable=False),
                    sa.Column('app_value_type_id', sa.Integer(), nullable=False),
                    sa.PrimaryKeyConstraint('event_data_key_id',
                                            name=op.f('pk_event_data_key')),
                    sa.UniqueConstraint('name',
                                        name=op.f('uq_event_data_key_name')))
    op.create_table('event_data',
                    sa.Column('event_data_id', sa.Integer(), nullable=False,
                              autoincrement=True),
                    sa.Column('event_id', sa.Integer(), nullable=False),
                    sa.Column('event_data_key_id', sa.Integer(),
                              nullable=False),
                    sa.Column('value_int', sa.BigInteger()),
                    sa.Column('value_real', sa.Float(precision=53)),
                    sa.Column('value_str', sa.String(length=500)),
                    sa.Column('units', sa.String(length=50), nullable=False,
                              server_default=''),
                    sa.ForeignKeyConstraint(['event_data_key_id'], [
                        'event_data_key.event_data_key_id'], name=op.f(
                        'fk_event_data_event_data_key_id_event_data_key')),
                    sa.ForeignKeyConstraint(['event_id'], ['event.event_id'],
                                            name=op.f(
                                                'fk_event_data_event_id_event')),
                    sa.PrimaryKeyConstraint('event_data_id',
                                            name=op.f('pk_event_data')),
                    sa.CheckConstraint('(value_int  IS NOT NULL '
                                       ' OR value_real IS NOT NULL '
                                       ' OR value_str  IS NOT NULL)',
                                       name=op.f(
                                           'ck_event_data_at_least_one_value')))
    op.add_column('data_sample_raw',
                  sa.Column('app_value_type_id', sa.Integer(), nullable=True))
    op.add_column('data_sample_raw', sa.Column('event_id', sa.Integer()))


    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_upgrade_ddl()
    else:
        print("Views were not created.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return
    op.alter_column('data_sample_raw',
                    sa.Column('data_sample_id', sa.Integer(),
                              autoincrement=True, nullable=False))

    op.create_primary_key(op.f('pk_data_sample_raw'), 'data_sample_raw',
                          ['data_sample_id'])
    op.create_foreign_key(op.f('fk_data_sample_raw_event_id_event'),
                          'data_sample_raw', 'event', ['event_id'],
                          ['event_id'])


def _postgresql_downgrade_ddl():
    """Drop the two event related functions:  add_event() and add_event_data()
    """
    op.execute(textwrap.dedent("""
        DROP FUNCTION add_event(
            TIMESTAMP WITHOUT TIME ZONE,
            CHARACTER VARYING,
            CHARACTER VARYING,
            CHARACTER VARYING,
            CHARACTER VARYING,
            CHARACTER VARYING);"""))
    op.execute(textwrap.dedent("""
        DROP FUNCTION add_event_data(
            INTEGER,
            CHARACTER VARYING,
            INTEGER,
            BIGINT,
            DOUBLE
            PRECISION,
            CHARACTER VARYING,
            CHARACTER VARYING);"""))


def downgrade():
    """Restore the 'event' and 'event_type' tables.  Drop the 'event_data'
    and 'event_data_key' tables and the event related functions
    """
    op.drop_constraint(op.f('fk_data_sample_raw_event_id_event'),
                       'data_sample_raw', type_='foreignkey')
    op.drop_constraint(op.f('pk_data_sample_raw'), 'data_sample_raw',
                       type_='primary')

    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_downgrade_ddl()
    else:
        print("Views were not dropped.  "
              "'%s' is not a supported database dialect." % db_dialect.name)
        return

    op.drop_column('data_sample_raw', 'app_value_type_id')
    op.drop_column('data_sample_raw', 'event_id')
    op.drop_column('data_sample_raw', 'data_sample_id')
    op.drop_table('event_data')
    op.drop_table('event_data_key')
    op.drop_table('event')
    op.create_table('event_type',
                    sa.Column('event_type_id', sa.INTEGER(), nullable=False),
                    sa.Column('name', sa.VARCHAR(length=250),
                              autoincrement=False, nullable=True),
                    sa.PrimaryKeyConstraint('event_type_id',
                                            name=op.f('pk_event_type')))
    op.create_table('event',
                    sa.Column('node_id', sa.Integer(), nullable=False),
                    sa.Column('event_type_id', sa.Integer(), nullable=False),
                    sa.Column('time_stamp', sa.DateTime(), nullable=False),
                    sa.Column('description', sa.Text(), nullable=False),
                    sa.ForeignKeyConstraint(['event_type_id'],
                                            ['event_type.event_type_id'],
                                            name=op.f(
                                                'fk_event_event_type_id_event_type')),
                    sa.ForeignKeyConstraint(['node_id'], ['node.node_id'],
                                            name=op.f('fk_event_node_id_node')),
                    sa.PrimaryKeyConstraint('node_id', 'event_type_id',
                                            'time_stamp',
                                            name=op.f('pk_event')))
