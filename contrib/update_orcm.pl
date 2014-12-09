#!/usr/bin/perl
#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# This script is now hardwired to support only GIT repos
#
# $HEADER$
#

use File::Find;
use File::Basename;
use File::Compare;
use File::Copy;
use File::Path;
use Getopt::Long;
use Text::Diff;
use Cwd;

my $ompi_arg;
my $orcm_arg;
my $ompi_dir;
my $orcm_dir;
my @ompi_files = ();
my @orcm_files = ();
my @ompi_tree = ();
my @orcm_tree = ();
my $help_arg = 0;
my $diff_file = "";
my $diff_arg;
my $update_arg;
my $modified_arg;
my $cmd;
my $cwd;
my $rev_arg;
my $rev=0;
  
my @ompi_global_ignores = ("oshmem");
my @ompi_leading_ignores = ("ompi", "test", "examples", "config/ompi", "orte/tools/orterun",
                            "orte/tools/wrappers", "orte/tools/orte-checkpoint",
                            "orte/tools/orte-clean", "orte/tools/orte-info",
                            "orte/tools/orte-migrate", "orte/tools/orte-ps",
                            "orte/tools/orte-restart", "orte/tools/orte-restart",
                            "orte/tools/orte-server", "orte/tools/orte-top",
                            ".gitignore", "Makefile.am", "README", "VERSION",
                            "autogen.pl", "configure.ac", "config/opal_mca.m4", "config/opal_setup_wrappers.m4",
                            "config/orte_config_files.m4", "contrib/platform",
                            "opal/threads", "opal/tools/wrappers",
                            "orte/mca/errmgr/default_app", "orte/mca/ess/env",
                            "orte/mca/ess/hnp", "orte/mca/ess/lsf",
                            "orte/mca/ess/singleton", "orte/mca/ess/slurm",
                            "orte/mca/ess/tm", "orte/mca/iof/mr", "orte/mca/plm/alps",
                            "orte/mca/plm/lsf", "orte/mca/plm/slurm", "orte/mca/plm/tm",
                            "orte/mca/state/app", "orte/mca/state/hnp", "orte/mca/state/novm",
                            "orte/mca/state/staged", "orte/tools/Makefile.am", "orte/util/proc_info.h",
                            ".mailmap", ".hgignore_global", "AUTHORS", "NEWS");


my @orcm_ignores = (".gitignore", ".hgignore_global", ".mailmap", "Makefile.am", "README", "VERSION",
                    "autogen.pl", "configure.ac", "config/opal_mca.m4",
                    "config/orcm", "config/scon", "opal/threads", "MANIFEST",
                    "config/orte_config_files.m4", "contrib",
                    "opal/tools/wrappers", "orte/tools/", "orte/util/proc_info.h",
                    "make.spec", "make_rpm.mk", "config/opal_check_ipmi.m4",
                    "contrib/database/", "contrib/dist/linux/open-rcm.spec", "ompi_sync_ref",
                    "AUTHORS", "orte/mca/ess/alps", "orte/mca/ess/singleton", "orte/mca/ess/tm",
                    "orte/mca/plm/alps", "orte/mca/plm/lsf", "orte/mca/plm/slurm");



# Command line parameters

my $ok = Getopt::Long::GetOptions("help|h" => \$help_arg,
                                  "ompi=s" => \$ompi_arg,
                                  "orcm=s" => \$orcm_arg,
                                  "diff=s" => \$diff_arg,
                                  "update" => \$update_arg,
                                  "update-modified" => \$modified_arg,
                                  "reverse" => \$rev_arg
    );

if (!$ok || $help_arg) {
    print "Invalid command line argument.\n\n"
        if (!$ok);
    print "Options:
  --diff | -diff       Output diff of changed files to specified file ('-' => stdout)
  --ompi | -ompi       Head of ompi repository
  --orcm | -orcm       Head of orcm repository
  --update | -update   Apply changes (add, delete, copy) to update target
  --update-modified    Only update modified files (do not add/delete files)
  --reverse | -reverse Reverse direction (merge orcm repo to ompi repo)\n";
    exit($ok ? 0 : 1);
}

if (!$ompi_arg || !$orcm_arg) {
    print "Missing ompi or orcm directory\n";
    exit(1);
}

if ($rev_arg) {
    $rev = 1;
}

if ($diff_arg) {
    if ($diff_arg eq "-") {
        *MYFILE = *STDOUT;
    } else {
        $diff_file = File::Spec->rel2abs($diff_arg);
        unlink ($diff_file);
        open(MYFILE, ">$diff_file");
    }
}

$cwd = cwd();
$ompi_dir = File::Spec->rel2abs($ompi_arg);
$orcm_dir = File::Spec->rel2abs($orcm_arg);

# get last sync hash
chdir($orcm_dir) or die "$!";
my $hashfile = "ompi_sync_ref";
open FILE, "<$hashfile"
    or die "could not open $hashfile: $!";
my $last_sync_hash = do { local $/; <FILE> };
close FILE;

# construct a tree of all files in the source directory tree
chdir($ompi_dir) or die "$!";
my $ompi_hash = `git log --pretty=format:'%h' -1`;

if (rev == 1) {
    $cmd = "git checkout $last_sync_hash";
    system($cmd);
}

@ompi_files = `git ls-files`;
chomp(@ompi_files);
# remove from the ompi tree all the areas we don't want
my $fnd;
my $ign;
foreach $ompi (@ompi_files) {
    $fnd = 0;
    # ignore anything in the ompi, oshmem, and test trees, and
    # any ompi config files
    foreach $ign (@ompi_global_ignores) {
        if (index($ompi,$ign) != -1) {
            $fnd = 1;
            last;
        }
    }
    if ($fnd == 0) {
        # check the leading ignores
        foreach $ign (@ompi_leading_ignores) {
            if (index($ompi,$ign) == 0) {
                $fnd = 1;
                last;
            }
        }
        if ($fnd == 0) {
            push(@ompi_tree, $ompi);
        }
    }
}
print "size of ompi_tree: " . @ompi_tree . ".\n";

# construct a tree of all files in the target directory tree
chdir($orcm_dir) or die "$!";
@orcm_files = `git ls-files`;
chomp(@orcm_files);
# remove from the orcm tree all the areas specific to ORCM
foreach $orcm (@orcm_files) {
    $fnd = 0;
    # ignore anything in the orcm, orcmapi, and scon trees, and
    # any orcm, orcmapi, and scon config files, and any orcm
    # components in the ORTE tree
    if (index($orcm,"orcm") != -1 || index($orcm,"scon") != -1) {
        # print "Ignoring " . $orcm . "\n";
        $fnd = 1;
    }
    if ($fnd == 0) {
        if ($rev == 1) {
            foreach $ign (@orcm_ignores) {
                if (index($orcm,$ign) == 0) {
                    $fnd = 1;
                    last;
                }
            }
        }
        if ($fnd == 0) {
            push(@orcm_tree, $orcm);
        }
    }
}
print "size of orcm_tree: " . @orcm_tree . ".\n";

# return to the working directory
chdir($cwd) or die "$!";

# print a list of files in the source tree that need to be added to the target
our $found;
our @modified = ();
our @src_pared = ();
our $srcpath;
our $tgtpath;
our @src_tree = ();
our @tgt_tree = ();
our $src_dir;
our %tgt_dir;
our $src;
our $tgt;

if($rev == 1) {
    @src_tree = @orcm_tree;
    @tgt_tree = @ompi_tree;
    $src_dir = $orcm_dir;
    $tgt_dir = $ompi_dir;

    update_tree();

    chdir($ompi_dir) or die "$!";

    my @ompi_changed_files = `git s`;
    if ($#ompi_changed_files > 0) {
        $cmd = "git stash";
        system($cmd);
    }
    $cmd = "git rebase master";
    system($cmd);
    if ($#ompi_changed_files > 0) {
        $cmd = "git stash pop";
        system($cmd);
    }
} else {
    @src_tree = @ompi_tree;
    @tgt_tree = @orcm_tree;
    $src_dir = $ompi_dir;
    $tgt_dir = $orcm_dir;

    update_tree();

    if ($update_arg) {
        chdir($orcm_dir) or die "$!";
        open FILE, "+>$hashfile"
            or die "could not open $hashfile: $!";
        print FILE "$ompi_hash";
        close FILE;
        $cmd = "git add $hashfile";
        system($cmd);
    }
}

# print a list of files that have been modified
foreach $tgt (@modified) {
    print "Modified: " . $tgt . "\n";
}
if ($diff_arg) {
    close(MYFILE);
}

sub update_tree
{
    chdir($tgt_dir) or die "ERROR: $!";
    foreach $src (@src_tree) {
        # print "Looking at src " . $src . "\n";
        $found = 0;
        foreach $tgt (@tgt_tree) {
            # print "Comparing to tgt " . $tgt . "\n";
            if ($src eq $tgt) {
                # printf "Matched: " . $src . " " . $tgt . "\n";
                # file has been found - ignore it
                $found = 1;
                $srcpath = File::Spec->catfile($src_dir, $src);
                $tgtpath = File::Spec->catfile($tgt_dir, $tgt);
                if (compare($srcpath, $tgtpath) != 0) {
                    if ($diff_arg) {
                        my $diff = diff $tgtpath, $srcpath, { STYLE => "Unified" };
                        print MYFILE $diff . "\n";
                        push(@modified, $src);
                    } elsif ($update_arg || $modified_arg) {
                        print "Updating $src to $tgt\n";
                        copy("$srcpath", "$tgtpath") or die "Copy failed: src=$srcpath tgt=$tgtpath\n";
                        $cmd = "git add -f $src";
                        system($cmd);
                    } else {
                        print "File " . $src . " has been modified\n";
                        push(@modified, $srcpath);
                    }
                }
                last;
                exit;
            }
        }
        if ($found == 0) {
            if (!$modified_arg) {
                if ($update_arg) {
                    $tgtpath = File::Spec->catfile($tgt_dir, $src);
                    my $tpath = dirname($tgtpath);
                    if (! -d $tpath) {
                        my $dirs = eval { mkpath($tpath) };
                        if (!$dirs) {
                            print "Failed to create path $tpath\n";
                            exit;
                        }
                    }
                    print "Adding $src_dir/$src to repo\n";
                    copy("$src_dir/$src", "$src") or die "Update failed: src=$src tgt=$src\n";
                    $cmd = "git add -f $src";
                    system($cmd);
                } else {
                    print "Add: " . $src . "\n";
                }
            } else {
                # print "Paring " . $src . "\n";
                push(@src_pared, $src);
            }
        }
    }
}
