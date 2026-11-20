/*
 * Copyright 2026 Sweet Meet
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.eleos.network;

/** One installed package, as the permission screen needs to see it. */
public final class AppRecord {
    public final String packageName;
    public final String label;
    public final boolean requestsInternet;

    public AppRecord(String packageName, String label, boolean requestsInternet) {
        this.packageName = packageName;
        this.label = label;
        this.requestsInternet = requestsInternet;
    }
}
