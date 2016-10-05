#
# Copyright (c) 2016 Intel Inc. All rights reserved
#
"""schema cleanup removing unused tables

Revision ID: 0fb66069a81f
Revises: d79399c4d070
Create Date: 2016-10-03 14:13:22.740117

"""

# revision identifiers, used by Alembic.
revision = '0fb66069a81f'
down_revision = 'd79399c4d070'
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap


def postgres_drop_tables():
    op.drop_table('maintenance_record')
    op.drop_table('fru')
    op.drop_table('fru_type')
    op.drop_table('data_sample')
    op.drop_table('job_node')
    op.drop_table('job')


def postgres_create_tables():
    op.create_table('fru_type',
        sa.Column('fru_type_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=250), nullable=False),
        sa.PrimaryKeyConstraint('fru_type_id', name=op.f('pk_fru_type')))
    op.create_table('fru',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('fru_type_id', sa.Integer(), nullable=False),
        sa.Column('fru_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('serial_number', sa.String(length=50), nullable=False),
        sa.ForeignKeyConstraint(['fru_type_id'], ['fru_type.fru_type_id'], name=op.f('fk_fru_fru_type_id_fru_type')),
        sa.ForeignKeyConstraint(['node_id'], ['node.node_id'], name=op.f('fk_fru_node_id_node')),
        sa.PrimaryKeyConstraint('node_id', 'fru_type_id', 'fru_id', name=op.f('pk_fr')))
    op.create_table('maintenance_record',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('fru_type_id', sa.Integer(), nullable=False),
        sa.Column('fru_id', sa.Integer(), nullable=False),
        sa.Column('replacement_date', sa.Date(), nullable=False),
        sa.Column('old_serial_number', sa.String(length=50), nullable=False),
        sa.Column('new_serial_number', sa.String(length=50), nullable=False),
        sa.ForeignKeyConstraint(['node_id', 'fru_type_id', 'fru_id'], ['fru.node_id', 'fru.fru_type_id', 'fru.fru_id'], 
        name=op.f('fk_maintenance_record_node_id_fr')),
        sa.PrimaryKeyConstraint('node_id', 'fru_type_id', 'fru_id', 'replacement_date', name=op.f('pk_maintenance_record')))
    op.create_table('data_sample',
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.Column('data_item_id', sa.Integer(), nullable=False),
        sa.Column('time_stamp', sa.DateTime(), nullable=False),
        sa.Column('value_int', sa.BigInteger()),
        sa.Column('value_real', sa.Float(precision=53)),
        sa.Column('value_str', sa.String(length=50)),
        sa.Column('units', sa.String(length=50)),
        sa.ForeignKeyConstraint(['data_item_id'], ['data_item.data_item_id'], name=op.f('fk_data_sample_data_item_id_data_item')),
        sa.ForeignKeyConstraint(['node_id'], ['node.node_id'], name=op.f('fk_data_sample_node_id_node')),
        sa.PrimaryKeyConstraint('node_id', 'data_item_id', 'time_stamp', name=op.f('pk_data_sample')),
        sa.CheckConstraint('(value_int  IS NOT NULL OR value_real IS NOT NULL OR value_str  IS NOT NULL)', 
        name=op.f('ck_data_sample_at_least_one_value')))
    op.create_table('job',
        sa.Column('job_id', sa.Integer(), autoincrement=True, unique=True, nullable=False),
        sa.Column('name', sa.String(length=100)),
        sa.Column('num_nodes', sa.Integer(), nullable=False),
        sa.Column('username', sa.String(length=75), nullable=False),
        sa.Column('time_submitted', sa.DateTime(), nullable=False),
        sa.Column('start_time', sa.DateTime()),
        sa.Column('end_time', sa.DateTime()),
        sa.Column('energy_usage', sa.Float()),
        sa.Column('exit_status', sa.Integer()),
        sa.PrimaryKeyConstraint('job_id', name=op.f('pk_job')))
    op.create_table('job_node',
        sa.Column('job_id', sa.Integer(), nullable=False),
        sa.Column('node_id', sa.Integer(), nullable=False),
        sa.ForeignKeyConstraint(['job_id'], ['job.job_id'], name=op.f('fk_job_node_job_id_job')),
        sa.ForeignKeyConstraint(['node_id'], ['node.node_id'], name=op.f('fk_job_node_node_id_node')),
        sa.PrimaryKeyConstraint('job_id', 'node_id', name=op.f('pk_job_node')))


def drop_tables():
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        postgres_drop_tables()
        return True
    else:
        print("Tables were not dropped. '%s' is not a supported database dialect." % db_dialect.name)
        return False


def create_tables():
    db_dialect = op.get_context().dialect
    if 'postgresql' in db_dialect.name:
        postgres_create_tables()
        return True
    else:
        print("Tables were not re-created. '%s' is not a supported database dialect." % db_dialect.name)
        return False


def upgrade():
    if not drop_tables():
        print("Error: DB Dialect is not supported, aborting upgrade!")
        return


def downgrade():
    if not create_tables():
        print("Error: DB Dialect is not supported, aborting downgrade!")
        return

