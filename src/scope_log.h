/*
 *   Copyright (C) 2019 Lukáš Karas <lukas.karas@centrum.cz>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#pragma once

#include <QtCore/QTextStream>

#include <string>
#include <chrono>

namespace timelapse {

/**
 * Simple tool that logs (debug) how long we spend in some scope.
 */
class ScopeLogger {
private:
  QTextStream *output;
  using Clock = std::chrono::steady_clock;
  Clock::time_point start{Clock::now()};
public:
  ScopeLogger(QTextStream *output, const QString &msg):
    output{output}
  {
    *output << msg << " ...";
    output->flush();
  };

  ~ScopeLogger(){

    using namespace std::chrono;
    auto duration = Clock::now() - start;
    *output <<
      QString(" %1 ms").arg(duration_cast<milliseconds>(duration).count()) <<
      endl;
  };

  ScopeLogger(const ScopeLogger &) = delete;
  ScopeLogger(ScopeLogger &&) = delete;
  ScopeLogger &operator=(const ScopeLogger &) = delete;
  ScopeLogger &operator=(ScopeLogger &&) = delete;

};
}
