# structual-support-for-cxx-concurrency
Implementation for draft D0642R0

Currently, there is little structural support in concurrent programming in C++. Based on the requirements in concurrent programming, this proposal intends to add structural support in concurrent programming in C++, enabling implementations to have robust architecture and flexible synchronization algorithm.
The issue is concluded into two major problems: the first is about the architecture in concurrent programming, and the second is about the algorithms for synchronizations.
The problems can be solved with a novel architecture and dedicated synchronization algorithms. With the support of the solution, not only are users able to structure concurrent programs like serial ones as flexible as function calls, but also to choose algorithms for synchronizations based on platform or performance considerations.
