#
# Copyright (c) 2016 Intel Corporation. All rights reserved
#
"""Update syslog view CSS-2562

Revision ID: f6d80b502c89
Revises: 36357b09c750
Create Date: 2016-05-03 11:33:56.328350

"""

# revision identifiers, used by Alembic.
revision = 'f6d80b502c89'
down_revision = '36357b09c750'
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap

def _postgresql_upgrade_ddl():
    """
    Drops old syslog view and creates the new one.
    """
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS syslog_view;"))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW syslog_view AS
            SELECT hostname, data_item, CAST(time_stamp AS TEXT), value_str AS log
            FROM data_sample_raw
            WHERE data_item LIKE 'syslog_log%';
    """))

def _postgresql_downgrade_ddl():
    """
    Drops new syslog view and creates the old one.
    """
    op.execute(textwrap.dedent("DROP VIEW IF EXISTS syslog_view;"))

    op.execute(textwrap.dedent("""
        CREATE OR REPLACE VIEW syslog_view AS
            SELECT (regexp_matches(value_str, '(<[0-9]*>[a-zA-Z]* [0-9]* [0-9]*:[0-9]*:[0-9]* )([a-zA-z0-9]*)', 'g'))[2] AS hostname,
                data_item, CAST(time_stamp AS TEXT), value_str AS log
            FROM data_sample_raw
            WHERE data_item LIKE 'syslog%';
    """))

def upgrade():
    """
    Upgrades syslog view as the way of logging syslog into data_sample_raw has changed.
    Regex is not longer required to extract the hostname, now it can be obtained directly
        from "hostname" field on data_sample_raw.
    "data_item" now needs to be searched for syslog_log% and not only for syslog%.
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
    Downgrades syslog view to the past way in which syslog was logged into data_sample_raw.
    Regex is used to extract the hostname as it was embeded into value_str.
    "data_item" was search for syslog% as the messages were not split.
    """
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        _postgresql_downgrade_ddl()
    else:
        print("Downgrade unsuccessful. "
            "'%s' is not a supported database dialect." % db_dialect.name)

    return
