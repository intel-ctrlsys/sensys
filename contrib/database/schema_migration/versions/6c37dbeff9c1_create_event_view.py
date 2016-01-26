#
# Copyright (c) 2016 Intel Inc. All rights reserved
#
"""create event view

Revision ID: 6c37dbeff9c1
Revises: 27089c78df35
Create Date: 2016-01-27 16:05:42.534187

"""

# revision identifiers, used by Alembic.
revision = '6c37dbeff9c1'
down_revision = '27089c78df35'
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap


sql_create_view_clauses = dict()

sql_create_view_clauses['postgresql'] = {
    'event_view' : textwrap.dedent("""
    CREATE OR REPLACE VIEW event_view AS
    SELECT CONCAT(event.event_id) as event_id,
           CONCAT(event.time_stamp) as time_stamp,
           event.severity,
           event.type,
           event.vendor,
           event.version,
           event.description,
           (SELECT event_data.value_str FROM event_data WHERE event_data_id = 1) as hostname
   FROM event, event_data
   WHERE event.event_id = event_data.event_id AND event_data.event_data_key_id = 1;
    """),
}

sql_drop_view_clauses = dict()

sql_drop_view_clauses['postgresql'] = {
    'event_view' : "DROP VIEW IF EXISTS event_view;"
}

def _create_view(view):
    dialect = op.get_context().dialect.name
    if dialect in sql_create_view_clauses and view in sql_create_view_clauses[dialect]:
        op.execute(sql_create_view_clauses[dialect][view])
        return True
    else:
        err_msg = ("View '{str_view}' not created. '{str_dialect}' "
                   "is not a supported database dialect")
        print(err_msg.format(str_view=view, str_dialect=dialect))

def _drop_view(view):
    dialect = op.get_context().dialect.name
    if dialect in sql_drop_view_clauses and view in sql_drop_view_clauses[dialect]:
        op.execute(sql_drop_view_clauses[dialect][view])
        return True
    else:
        err_msg = ("Unable to drop View '{str_view}' "
                   "is not a supported database dialect")
        print(err_msg.format(str_view=view, str_dialect=dialect))
        return False

def upgrade():
    _create_view('event_view')

def downgrade():
    _drop_view('event_view')
