#
# Copyright (c) 2015 Intel Inc. All rights reserved
#
"""Increased data_sample_raw:value_str and data_sample:value_str from 50 to 500.

Revision ID: 13fcd621c9d5
Revises: 241f4694b528
Create Date: 2015-10-15 12:55:18.272003

"""

# revision identifiers, used by Alembic.
revision = '13fcd621c9d5'
down_revision = '241f4694b528'
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap

def postgres_drop_data_samples_view():
    op.execute("DROP VIEW IF EXISTS data_samples_view;")

def postgres_recreate_data_samples_view():
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

def drop_data_samples_view():
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        postgres_drop_data_samples_view()
        return True
    else:
        print("Views were not dropped. '%s' is not a supported database dialect." % db_dialect.name)
        return False

def recreate_data_samples_view():
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        postgres_recreate_data_samples_view()
        return True
    else:
        print("Views were not re-created. '%s' is not a supported database dialect." % db_dialect.name)
        return False

def upgrade():
    if not drop_data_samples_view():
        print("Error: DB Dialect is not supported, aborting upgrade!")
        return
    op.alter_column('data_sample_raw', 'value_str',
               existing_type=sa.VARCHAR(length=50),
               type_=sa.String(length=500),
               existing_nullable=True)
    op.alter_column('data_sample', 'value_str',
               existing_type=sa.VARCHAR(length=50),
               type_=sa.String(length=500),
               existing_nullable=True)
    recreate_data_samples_view()


def downgrade():
    if not drop_data_samples_view():
        print("Error: DB Dialect is not supported, aborting downgrade!")
        return
    op.alter_column('data_sample', 'value_str',
               existing_type=sa.String(length=500),
               type_=sa.VARCHAR(length=50),
               existing_nullable=True)
    op.alter_column('data_sample_raw', 'value_str',
               existing_type=sa.String(length=500),
               type_=sa.VARCHAR(length=50),
               existing_nullable=True)
    recreate_data_samples_view()

