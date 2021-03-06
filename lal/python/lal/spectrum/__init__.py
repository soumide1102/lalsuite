# Copyright (C) 2013 Duncan Macleod
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

## \addtogroup lal_py_spectrum
"""This module provides spectral estimation methods, using wrappings
of the LAL FFT module
"""
# \author Duncan Macleod (<duncan.macleod@ligo.org>)
#
# ### Synopsis ###
#
# ~~~
# from lal import spectrum
# ~~~
#
# ### Example ###
#
# \code
# from lal import spectrum
# psd = spectrum.median_mean(timeseries, 16384, 8192)
# \endcode

from .. import git_version
__author__ = "Duncan Macleod <duncan.macleod@ligo.org>"
__version__ = git_version.id
__date__ = git_version.date

from .averagespectrum import *
from .distributions import *
