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


sql_create_item_clauses = dict()

sql_create_item_clauses['postgresql'] = {
    'event_view' : textwrap.dedent("""
    CREATE OR REPLACE VIEW event_view AS
    SELECT CONCAT(event.event_id) as event_id,
           CONCAT(event.time_stamp) as time_stamp,
           event.severity,
           event.type,
           event_data.value_str as hostname,
           get_event_msg(event.event_id) as event_message
   FROM event
   JOIN event_data ON event.event_id = event_data.event_id AND event_data.event_data_key_id = 1;
    """),
    'get_event_msg_function' : textwrap.dedent("""
    CREATE OR REPLACE FUNCTION get_event_msg(key INTEGER) RETURNS VARCHAR as $$
    DECLARE
        message VARCHAR;
    BEGIN
        SELECT value_str INTO message
        FROM event_data
        WHERE event_id = key AND event_data_key_id = 4;
        RETURN message;
    END; $$
    LANGUAGE plpgsql;
    """),
}

sql_drop_item_clauses = dict()

sql_drop_item_clauses['postgresql'] = {
    'event_view' : "DROP VIEW IF EXISTS event_view;",
    'get_event_msg_function' : "DROP FUNCTION IF EXISTS get_event_msg(INTEGER);"
}

def _create_item(item):
    dialect = op.get_context().dialect.name
    if dialect in sql_create_item_clauses and item in sql_create_item_clauses[dialect]:
        op.execute(sql_create_item_clauses[dialect][item])
        return True
    else:
        err_msg = ("Item '{str_item}' not created. '{str_dialect}' "
                   "is not a supported database dialect")
        print(err_msg.format(str_item=item, str_dialect=dialect))

def _drop_item(item):
    dialect = op.get_context().dialect.name
    if dialect in sql_drop_item_clauses and item in sql_drop_item_clauses[dialect]:
        op.execute(sql_drop_item_clauses[dialect][item])
        return True
    else:
        err_msg = ("Unable to drop item '{str_item}' "
                   "is not a supported database dialect")
        print(err_msg.format(str_item=item, str_dialect=dialect))
        return False

def upgrade():
    _create_item('get_event_msg_function')
    _create_item('event_view')

def downgrade():
    _drop_item('event_view')
    _drop_item('get_event_msg_function')
