#
# Copyright (c) 2015 Intel Inc. All rights reserved
#
"""Creating views for octl query command (data_sensors_view, syslog_view, nodes_idle_time_view)

Revision ID: 1d10ef20817b
Revises: 13fcd621c9d5
Create Date: 2015-11-11 20:27:19.275396

"""

# revision identifiers, used by Alembic.
revision = '1d10ef20817b'
down_revision = '13fcd621c9d5'
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap


sql_create_view_clauses = dict()

sql_create_view_clauses['postgresql'] = {
    'data_sensors_view' : textwrap.dedent("""
        CREATE OR REPLACE VIEW data_sensors_view AS
            SELECT dsr.hostname AS hostname,
                   dsr.data_item AS data_item,
                   CONCAT(dsr.time_stamp) AS time_stamp,
                   CONCAT(dsr.value_int, dsr.value_real, dsr.value_str) AS value_str,
                   dsr.units AS units
               FROM data_sample_raw AS dsr
                   LIMIT 100;
    """),
    'syslog_view' : textwrap.dedent("""
        CREATE OR REPLACE VIEW syslog_view AS
            SELECT (regexp_matches(syslog.value_str, '(<[0-9]*>[a-zA-Z]* [0-9]* [0-9]*:[0-9]*:[0-9]* )([a-zA-z0-9]*)', 'g'))[2] AS hostname,
                   syslog.data_item AS data_item,
                   concat(syslog.time_stamp) AS time_stamp,
                   syslog.value_str AS log
              FROM data_sample_raw AS syslog
                   WHERE syslog.data_item LIKE 'syslog%';
    """),
    'nodes_idle_time_view' : textwrap.dedent("""
        CREATE OR REPLACE VIEW nodes_idle_time_view AS
            SELECT data_sample_raw.hostname,
                   max(data_sample_raw.time_stamp) AS last_reading,
                   now() - max(data_sample_raw.time_stamp) AS idle_time,
                   concat(now() - max(data_sample_raw.time_stamp)) AS idle_time_str
              FROM data_sample_raw
                GROUP BY data_sample_raw.hostname;
    """),
}

sql_drop_view_clauses = dict()

sql_drop_view_clauses['postgresql'] = {
   'data_sensors_view'    : "DROP VIEW IF EXISTS data_sensors_view;",
   'syslog_view'          : "DROP VIEW IF EXISTS syslog_view;",
   'nodes_idle_time_view' : "DROP VIEW IF EXISTS nodes_idle_time_view;",
}


def _create_view(view):
    dialect = op.get_context().dialect.name
    if dialect in sql_create_view_clauses and view in sql_create_view_clauses[dialect]:
        op.execute(sql_create_view_clauses[dialect][view])
        return True
    else:
        print("View '{str_view}' not created. '{str_dialect}' is not a supported database dialect." . format(str_view=view, str_dialect=dialect))
        return False

def _drop_view(view):
    dialect = op.get_context().dialect.name
    if dialect in sql_drop_view_clauses and view in sql_drop_view_clauses[dialect]:
        op.execute(sql_drop_view_clauses[dialect][view])
        return True
    else:
        print("Unable to drop view '{str_view}'. '{str_dialect}' is not a supported database dialect." . format(str_view=view, str_dialect=dialect))
        return False

def upgrade():
    _create_view('data_sensors_view')
    _create_view('syslog_view')
    _create_view('nodes_idle_time_view')

def downgrade():
    _drop_view('nodes_idle_time_view')
    _drop_view('syslog_view')
    _drop_view('data_sensors_view')
