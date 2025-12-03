#!/usr/bin/env perl

use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

# Initialize test cluster
my $node = PostgreSQL::Test::Cluster->new('ssk_test');
$node->init;
$node->start;

# Create extension
$node->safe_psql('postgres', 'CREATE EXTENSION ssk;');

# Test basic UDT
my $result = $node->safe_psql('postgres', "SELECT ssk_out(ssk_in('stub'))");
ok($result =~ /ssk_output_stub/, 'UDT in/out works');

# Test aggregate
my $result = $node->safe_psql('postgres', "SELECT ssk_agg(id) FROM (VALUES (1), (2)) AS t(id);");
ok($result =~ /<ssk binary>/, 'Aggregate creates SSK');

# Test keystore/memory management (placeholder)
# Simulate large SSK to test eviction
# Add memory pressure tests

done_testing();