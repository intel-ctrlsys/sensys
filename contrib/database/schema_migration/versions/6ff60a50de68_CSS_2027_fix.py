#
# Copyright (c) 2016 Intel Inc. All rights reserved
#
"""(CSS-2027 fix) Basic fix for segfault on sensor get inventory command, due to bad
   data type handling on no string types. A better fix will be needed in the near future.

Revision ID: 6ff60a50de68
Revises: 6dccea132086
Create Date: 2016-02-25 15:54:04.196011

"""

# revision identifiers, used by Alembic.
revision = '6ff60a50de68'
down_revision = '6dccea132086'
branch_labels = None

from alembic import op
import sqlalchemy as sa
import textwrap

sql_create_view_clauses = dict()
sql_create_view_clauses['postgresql'] = {
    'node_features_view' : textwrap.dedent("""
        CREATE OR REPLACE VIEW node_features_view AS
            SELECT CAST(node_feature.node_id AS TEXT) AS node_id,
                   node.hostname,
                   CAST(node_feature.feature_id AS TEXT) AS feature_id,
                   feature.name AS feature,
                   COALESCE('') AS value_int,
                   COALESCE('') AS value_real,
                   CONCAT(CAST(value_int AS TEXT), CAST(value_real AS TEXT), value_str) AS value_str,
                   node_feature.units
                FROM node_feature
                     JOIN node ON node.node_id = node_feature.node_id
                     JOIN feature ON feature.feature_id = node_feature.feature_id;
    """),
    'node_features_view_old' : textwrap.dedent("""
        CREATE OR REPLACE VIEW node_features_view AS
            SELECT node_feature.node_id,
                   node.hostname,
                   node_feature.feature_id,
                   feature.name AS feature,
                   node_feature.value_int,
                   node_feature.value_real,
                   node_feature.value_str,
                   node_feature.units
                FROM node_feature
                     JOIN node ON node.node_id = node_feature.node_id
                     JOIN feature ON feature.feature_id = node_feature.feature_id;
    """),
}

sql_drop_view_clauses = dict()
sql_drop_view_clauses['postgresql'] = {
   'node_features_view' : "DROP VIEW IF EXISTS node_features_view;",
}

def _create_view(view):
    dialect = op.get_context().dialect.name
    if dialect in sql_create_view_clauses and view in sql_create_view_clauses[dialect]:
        op.execute(sql_create_view_clauses[dialect][view])
        return True
    else:
        print("View '{str_view}' not created. '{str_dialect}' is not a supported database dialect.".format(str_view=view, str_dialect=dialect))
        return False

def _drop_view(view):
    dialect = op.get_context().dialect.name
    if dialect in sql_drop_view_clauses and view in sql_drop_view_clauses[dialect]:
        op.execute(sql_drop_view_clauses[dialect][view])
        return True
    else:
        print("Unable to drop view '{str_view}'. '{str_dialect}' is not a supported database dialect.".format(str_view=view, str_dialect=dialect))
        return False

def upgrade():
    _drop_view('node_features_view')
    _create_view('node_features_view')

def downgrade():
    _drop_view('node_features_view')
    _create_view('node_features_view_old')
