#
# Copyright (c) 2016 Intel Corporation. All rights reserved
#
"""Views for req 23g

Revision ID: 6dccea132086
Revises: 6c37dbeff9c1
Create Date: 2016-02-19 11:06:03.712253

"""

# revision identifiers, used by Alembic.
revision = '6dccea132086'
down_revision = '6c37dbeff9c1'
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap

sql_create_view_clauses = dict()

sql_create_view_clauses['postgresql'] = {
    'event_date_view' : textwrap.dedent("""
        CREATE OR REPLACE VIEW event_date_view AS
            SELECT CAST(event_id AS TEXT) AS event_id,
                   CAST(time_stamp AS TEXT) AS time_stamp
                FROM event;
    """),
    'event_sensor_data_view' : textwrap.dedent("""
        CREATE OR REPLACE VIEW event_sensor_data_view AS
            SELECT CAST(time_stamp AS TEXT) AS time_stamp,
                   hostname,
                   data_item,
                   CONCAT(CAST(value_int AS TEXT), CAST(value_real AS TEXT), value_str) AS value,
                   units
                FROM data_sample_raw;
    """),
}

sql_drop_view_clauses = dict()

sql_drop_view_clauses['postgresql'] = {
   'event_date_view'        : "DROP VIEW IF EXISTS event_date_view;",
   'event_sensor_data_view' : "DROP VIEW IF EXISTS event_sensor_data_view;",
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
    _create_view('event_date_view')
    _create_view('event_sensor_data_view')

def downgrade():
    _drop_view('event_date_view')
    _drop_view('event_sensor_data_view')
